from contextlib import redirect_stderr, redirect_stdout
from datetime import datetime, timedelta, timezone
from http.client import HTTPConnection
from io import BytesIO, StringIO
import json
import os
from pathlib import Path
import tempfile
from threading import Event, Thread
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlencode, urlsplit

from exodus import steam_sync as sync
from exodus.steam_catalog import steam_entry


ACCOUNT = "76561198000000000"
API_KEY = "1" * 32
APP_INFO = {"response": {"game_count": 2, "games": [
    {"appid": 440, "name": "  Synthetic Team  "},
    {"appid": 10, "name": "\u661f Test"},
]}}


def assertion(return_to, age=0):
    time = datetime.now(timezone.utc) - timedelta(seconds=age)
    identity = f"https://steamcommunity.com/openid/id/{ACCOUNT}"
    return {
        "openid.ns": sync.OPENID_NS,
        "openid.mode": "id_res",
        "openid.op_endpoint": sync.OPENID_PROVIDER,
        "openid.claimed_id": identity,
        "openid.identity": identity,
        "openid.return_to": return_to,
        "openid.response_nonce": time.strftime("%Y-%m-%dT%H:%M:%SZ") + "synthetic",
        "openid.assoc_handle": "synthetic association",
        "openid.signed": ",".join(sorted(sync.REQUIRED_SIGNATURE_FIELDS)),
        "openid.sig": "synthetic signature",
    }


class FakeSteam:
    def __init__(self, library=APP_INFO, valid=True):
        self.library = library
        self.valid = valid
        self.requests = []

    def open(self, request, *, timeout):
        self.requests.append(request)
        endpoint = request.full_url.split("?")[0]
        if endpoint == sync.OPENID_PROVIDER:
            verdict = "true" if self.valid else "false"
            return BytesIO(f"ns:{sync.OPENID_NS}\nis_valid:{verdict}\n".encode("ascii"))
        if endpoint != sync.OWNED_GAMES:
            raise AssertionError("unexpected Steam endpoint")
        if isinstance(self.library, Exception):
            raise self.library
        if isinstance(self.library, bytes):
            return BytesIO(self.library)
        return BytesIO(json.dumps(self.library).encode("utf-8"))


class LocalBrowser:
    def __init__(self, signed_callback=assertion, wrong_path=False):
        self.signed_callback = signed_callback
        self.wrong_path = wrong_path
        self.start_url = None
        self.callback_fields = None
        self.replies = []
        self.worker = None
        self.error = None

    def open(self, address):
        self.start_url = address
        params = parse_qs(urlsplit(address).query)
        return_to = params["openid.return_to"][0]
        self.callback_fields = self.signed_callback(return_to)
        self.worker = Thread(target=self.finish_sign_in, args=(return_to,), daemon=True)
        self.worker.start()
        return True

    def finish_sign_in(self, return_to):
        callback = urlsplit(return_to)
        connection = HTTPConnection(callback.hostname, callback.port, timeout=4)
        try:
            paths = [callback.path]
            if self.wrong_path:
                paths.insert(0, "/steam/callback/incorrect")
            for path in paths:
                connection.request("GET", path + "?" + urlencode(self.callback_fields))
                response = connection.getresponse()
                self.replies.append((response.status, response.read(), response.getheader("Cache-Control")))
        except Exception as error:
            self.error = error
        finally:
            connection.close()

    def join(self):
        if self.worker is None:
            return
        self.worker.join(timeout=5)
        if self.worker.is_alive():
            raise AssertionError("local callback did not terminate")
        if self.error is not None:
            raise self.error


class SteamSyncTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.output = Path(directory.name) / "catalog.json"

    def run_sync(self, provider, browser, base=None):
        self.addCleanup(browser.join)
        count = sync.sync_library(self.output, API_KEY, base_catalog=base, timeout=2,
                                  open_browser=browser.open, opener=provider)
        browser.join()
        return count

    def test_sign_in_and_refresh_steam_rows_in_existing_home_catalog(self):
        homebrew = {**steam_entry("tools/local.nro", "Local Homebrew"), "provider": "switch_homebrew"}
        self.output.write_text(json.dumps({
            "schema_version": 1, "entries": [steam_entry("77", "Prior Steam account"), homebrew],
        }), encoding="utf-8")
        provider = FakeSteam()
        browser = LocalBrowser(wrong_path=True)
        captured_stderr = StringIO()
        with redirect_stderr(captured_stderr):
            count = self.run_sync(provider, browser, self.output)

        self.assertEqual(count, 2)
        self.assertEqual(captured_stderr.getvalue(), "")
        self.assertEqual([reply[0] for reply in browser.replies], [404, 200])
        self.assertEqual(browser.replies[-1][2], "no-store")
        self.assertNotIn(ACCOUNT.encode(), browser.replies[-1][1])
        self.assertEqual(browser.start_url.split("?")[0], sync.OPENID_PROVIDER)
        auth = parse_qs(urlsplit(browser.start_url).query)
        target = urlsplit(auth["openid.return_to"][0])
        self.assertEqual(target.scheme, "http")
        self.assertEqual(target.hostname, "127.0.0.1")
        self.assertTrue(target.path.startswith("/steam/callback/"))
        self.assertEqual(auth["openid.realm"], [f"http://127.0.0.1:{target.port}/"])
        self.assertNotIn(API_KEY, browser.start_url)

        verify, books = provider.requests
        self.assertEqual(verify.full_url, sync.OPENID_PROVIDER)
        self.assertEqual(verify.get_method(), "POST")
        expected_fields = {**browser.callback_fields, "openid.mode": "check_authentication"}
        self.assertEqual(parse_qs(verify.data.decode("ascii")),
                         {key: [value] for key, value in expected_fields.items()})
        self.assertEqual(books.full_url.split("?")[0], sync.OWNED_GAMES)
        self.assertEqual(books.get_method(), "GET")
        lookup = parse_qs(urlsplit(books.full_url).query)
        self.assertEqual(lookup["key"], [API_KEY])
        self.assertEqual(json.loads(lookup["input_json"][0]), {"steamid": int(ACCOUNT), "include_appinfo": True})
        raw = self.output.read_text(encoding="utf-8")
        self.assertNotIn(ACCOUNT, raw)
        self.assertNotIn(API_KEY, raw)
        self.assertEqual(json.loads(raw), {"schema_version": 1, "entries": [
            steam_entry("10", "\u661f Test"), steam_entry("440", "Synthetic Team"), homebrew,
        ]})

    def test_unverified_sign_in_preserves_snapshot(self):
        self.output.write_bytes(b"prior snapshot")
        provider = FakeSteam(valid=False)
        browser = LocalBrowser()
        with self.assertRaisesRegex(sync.SteamSyncError, "could not verify"):
            self.run_sync(provider, browser)
        self.assertEqual(len(provider.requests), 1)
        self.assertEqual(self.output.read_bytes(), b"prior snapshot")

    def test_refreshes_bom_marked_home_catalogs_and_preserves_unicode(self):
        homebrew = {
            **steam_entry("tools/Игры.nro", "拡張ゲーム 🎮"), "provider": "switch_homebrew",
        }
        catalog = {"schema_version": 1, "entries": [steam_entry("77", "Old Steam"), homebrew]}
        text = json.dumps(catalog, ensure_ascii=False)
        for encoding, bom in (("utf-8", b"\xef\xbb\xbf"), ("utf-16-le", b"\xff\xfe")):
            with self.subTest(encoding=encoding):
                self.output.write_bytes(bom + text.encode(encoding))
                provider = FakeSteam()
                browser = LocalBrowser()
                self.assertEqual(self.run_sync(provider, browser, self.output), 2)
                self.assertEqual(browser.replies[0][0], 200)
                refreshed = self.output.read_bytes()
                self.assertFalse(refreshed.startswith(bom))
                self.assertEqual(json.loads(refreshed.decode("utf-8")), {
                    "schema_version": 1,
                    "entries": [steam_entry("10", "\u661f Test"),
                                steam_entry("440", "Synthetic Team"), homebrew],
                })

    def test_requires_matching_signed_identity_and_fresh_nonce(self):
        return_to = "http://127.0.0.1:32187/steam/callback/synthetic"
        bad_values = {
            "openid.ns": "other",
            "openid.op_endpoint": "https://elsewhere.invalid/",
            "openid.return_to": return_to + "/different",
            "openid.claimed_id": "https://elsewhere.invalid/openid/id/" + ACCOUNT,
            "openid.identity": "https://steamcommunity.com/openid/id/76561198000000001",
            "openid.signed": "assoc_handle",
            "openid.sig": "",
            "openid.response_nonce": "not a time stamp",
        }
        provider = FakeSteam()
        for key, invalid in bad_values.items():
            with self.subTest(field=key), self.assertRaises(sync.SteamSyncError):
                sync.verify_openid({**assertion(return_to), key: invalid}, return_to, provider)
        with self.assertRaisesRegex(sync.SteamSyncError, "expired"):
            sync.verify_openid(assertion(return_to, age=900), return_to, provider)
        self.assertEqual(provider.requests, [])

    def test_accepts_documented_http_steam_identifier_and_timestamp_only_nonce(self):
        return_to = "http://127.0.0.1:32187/steam/callback/synthetic"
        fields = assertion(return_to)
        identifier = f"http://steamcommunity.com/openid/id/{ACCOUNT}"
        fields["openid.claimed_id"] = identifier
        fields["openid.identity"] = identifier
        fields["openid.response_nonce"] = fields["openid.response_nonce"][:20]
        self.assertEqual(sync.verify_openid(fields, return_to, FakeSteam()), ACCOUNT)

    def test_cancellation_timeout_and_browser_unavailable(self):
        provider = FakeSteam()
        browser = LocalBrowser(lambda return_to: {"openid.mode": "cancel"})
        with self.assertRaisesRegex(sync.SteamSyncError, "cancelled"):
            self.run_sync(provider, browser)
        with self.assertRaisesRegex(sync.SteamSyncError, "Timed out"):
            sync.sign_in(provider, lambda address: True, timeout=0.01)
        with self.assertRaisesRegex(sync.SteamSyncError, "Could not open"):
            sync.sign_in(provider, lambda address: False, timeout=1)
        with patch("exodus.steam_sync.webbrowser.open", side_effect=OSError("synthetic launch error")) as failed:
            with self.assertRaisesRegex(sync.SteamSyncError, "Could not open"):
                sync.sign_in(provider, failed, timeout=1)
        self.assertFalse(self.output.exists())
        self.assertEqual(provider.requests, [])

    def test_callback_can_finish_while_foreground_launcher_waits(self):
        provider = FakeSteam()
        browser = LocalBrowser()
        release = Event()
        launch_finished = Event()
        self.addCleanup(browser.join)

        def foreground_browser(address):
            try:
                opened = browser.open(address)
                browser.join()
                release.wait(timeout=5)
                return opened
            finally:
                launch_finished.set()

        try:
            count = sync.sync_library(self.output, API_KEY, timeout=2, opener=provider,
                                      open_browser=foreground_browser)
            self.assertEqual(count, 2)
            browser.join()
            self.assertFalse(launch_finished.is_set())
            self.assertEqual(browser.replies[0][0], 200)
            self.assertTrue(self.output.is_file())
        finally:
            release.set()
        self.assertTrue(launch_finished.wait(timeout=2))

    def test_foreground_launcher_does_not_delay_timeout(self):
        provider = FakeSteam()
        launch_started = Event()
        launch_finished = Event()
        release = Event()

        def slow_browser(address):
            launch_started.set()
            release.wait(timeout=2)
            launch_finished.set()
            return True

        try:
            with self.assertRaisesRegex(sync.SteamSyncError, "Timed out"):
                sync.sign_in(provider, slow_browser, timeout=0.05)
            self.assertTrue(launch_started.wait(timeout=1))
            self.assertFalse(launch_finished.is_set())
        finally:
            release.set()
        self.assertTrue(launch_finished.wait(timeout=1))
        self.assertEqual(provider.requests, [])

    def test_rejects_duplicate_callback_fields(self):
        with self.assertRaisesRegex(sync.SteamSyncError, "Malformed"):
            sync.parse_openid_response("openid.mode=id_res&openid.mode=cancel")
        with self.assertRaisesRegex(sync.SteamSyncError, "Malformed"):
            sync.parse_openid_response("openid.mode=id_res&invalid")

    def test_inaccessible_library_preserves_output_while_explicitly_empty_is_written(self):
        self.output.write_bytes(b"prior snapshot")
        with self.assertRaisesRegex(sync.SteamSyncError, "did not expose"):
            self.run_sync(FakeSteam({"response": {}}), LocalBrowser())
        self.assertEqual(self.output.read_bytes(), b"prior snapshot")
        self.assertEqual(self.run_sync(FakeSteam({"response": {"game_count": 0}}), LocalBrowser()), 0)
        self.assertEqual(json.loads(self.output.read_bytes()), {"schema_version": 1, "entries": []})

    def test_library_denial_reports_no_raw_key_or_request(self):
        error = HTTPError(sync.OWNED_GAMES + "?key=" + API_KEY, 403, API_KEY,
                          None, BytesIO(ACCOUNT.encode()))
        with self.assertRaisesRegex(sync.SteamSyncError, "rejected library access") as caught:
            sync.fetch_library(FakeSteam(error), API_KEY, ACCOUNT)
        self.assertNotIn(API_KEY, str(caught.exception))
        self.assertNotIn(ACCOUNT, str(caught.exception))

    def test_rejects_malformed_or_oversized_library_responses(self):
        inputs = [
            b'{"response":{"game_count":0,"game_count":1}}',
            b'{"response":{"game_count":0,"extra":NaN}}',
            b"\xff\xfe" + json.dumps(APP_INFO).encode("utf-16-le"),
            {"response": {"game_count": False, "games": []}},
            {"response": {"game_count": 1}},
            {"response": {"game_count": 1, "games": [{"appid": True}]}},
            {"response": {"game_count": 1, "games": [{"appid": 10, "name": "\ud800"}]}},
            {"response": {"game_count": 2, "games": [{"appid": 10}, {"appid": 10}]}},
            {"response": {"game_count": sync.MAX_CATALOG_ENTRIES + 1, "games": []}},
        ]
        for body in inputs:
            with self.subTest(body=body), self.assertRaises(sync.SteamSyncError):
                sync.fetch_library(FakeSteam(body), API_KEY, ACCOUNT)
        with self.assertRaisesRegex(sync.SteamSyncError, "size limit"):
            sync.request_steam(FakeSteam(b"abcd"), sync.Request(sync.OWNED_GAMES),
                               limit=3, operation="library sync")

    def test_base_catalog_is_validated_before_sign_in(self):
        self.output.write_text('{"schema_version":1,"entries":[{}]}', encoding="utf-8")
        with patch("exodus.steam_sync.webbrowser.open") as browser, self.assertRaisesRegex(
                sync.SteamSyncError, "invalid entry"):
            sync.sync_library(self.output, API_KEY, base_catalog=self.output, opener=FakeSteam())
        browser.assert_not_called()

    def test_invalid_utf16_base_catalogs_preserve_output_before_sign_in(self):
        text = '{"schema_version":1,"entries":[]}'
        bad_name = json.dumps({"schema_version": 1, "entries": [steam_entry("10", "\ud800")]},
                              ensure_ascii=False)
        invalid = [
            b"\xff\xfe" + text.encode("utf-16-le")[:-1],
            b"\xff\xfe" + bad_name.encode("utf-16-le", errors="surrogatepass"),
            b"\xff\xfe" + '{"schema_version":1,"entries":[],"entries":[]}'.encode("utf-16-le"),
            text.encode("utf-16-le"),
            b"\xfe\xff" + text.encode("utf-16-be"),
            b"\xff\xfe\x00\x00" + text.encode("utf-32-le"),
        ]
        for data in invalid:
            with self.subTest(data=data):
                self.output.write_bytes(data)
                with patch("exodus.steam_sync.webbrowser.open") as browser, self.assertRaisesRegex(
                        sync.SteamSyncError, "Invalid JSON"):
                    sync.sync_library(self.output, API_KEY, base_catalog=self.output, opener=FakeSteam())
                browser.assert_not_called()
                self.assertEqual(self.output.read_bytes(), data)

    def test_utf16_base_catalog_limit_counts_encoded_bytes(self):
        data = b"\xff\xfe" + '{"schema_version":1,"entries":[]}'.encode("utf-16-le")
        self.output.write_bytes(data)
        with patch("exodus.steam_sync.MAX_CATALOG_BYTES", len(data)):
            self.assertEqual(sync.read_base_catalog(self.output), [])
        with patch("exodus.steam_sync.MAX_CATALOG_BYTES", len(data) - 1), self.assertRaisesRegex(
                sync.SteamSyncError, "size limit"):
            sync.read_base_catalog(self.output)

    def test_failed_atomic_replace_preserves_output_and_cleans_stage(self):
        self.output.write_bytes(b"prior snapshot")
        with patch("exodus.steam_sync.os.replace", side_effect=PermissionError), self.assertRaisesRegex(
                sync.SteamSyncError, "previous output was preserved"):
            sync.save_catalog(self.output, [steam_entry("10", "Test")])
        self.assertEqual(self.output.read_bytes(), b"prior snapshot")
        self.assertEqual(list(self.output.parent.iterdir()), [self.output])

    def test_cli_removes_api_key_from_browser_environment(self):
        def sync_in_process(output, key, **kwargs):
            self.assertEqual(key, API_KEY)
            self.assertNotIn(sync.KEY_ENV, os.environ)
            return 1

        with patch.dict(os.environ, {sync.KEY_ENV: API_KEY}), patch(
                "exodus.steam_sync.sync_library", side_effect=sync_in_process), redirect_stdout(StringIO()):
            self.assertEqual(sync.main(["--output", str(self.output)]), 0)
        with patch.dict(os.environ, {sync.KEY_ENV: ""}), patch(
                "exodus.steam_sync.webbrowser.open") as browser, redirect_stderr(StringIO()) as stderr:
            self.assertEqual(sync.main(["--output", str(self.output)]), 1)
        self.assertIn(sync.KEY_ENV, stderr.getvalue())
        browser.assert_not_called()


if __name__ == "__main__":
    unittest.main()
