from contextlib import redirect_stderr, redirect_stdout
from email.message import Message
from http.server import BaseHTTPRequestHandler, HTTPServer
from io import BytesIO, StringIO
import json
import os
from ssl import SSLError
from threading import Thread
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlsplit
from urllib.request import HTTPSHandler, ProxyHandler, Request, build_opener
from urllib.response import addinfourl

from exodus import steam_entitlement as entitlement
from exodus import steam_sync as sync
from tests.test_steam_sync import ACCOUNT, LocalBrowser, assertion


APP_ID = 730
OTHER_APP_ID = 731
OTHER_ACCOUNT = "76561198000000001"
PUBLISHER_KEY = "0123456789abcdef" * 2
PARTNER_ENDPOINT = "https://partner.steam-api.com/ISteamUser/CheckAppOwnership/v4/"
SCOPE_ENV = "EXODUS_STEAM_PUBLISHER_APP_ID"
KEY_ENV = "STEAMWORKS_PUBLISHER_KEY"
Status = entitlement.LicenseStatus


def http_reply(request, body, code=200, fields=None):
    headers = Message()
    if fields is None:
        fields = (("Content-Length", str(len(body))),)
    for name, value in fields:
        headers[name] = value
    response = addinfourl(BytesIO(body), headers, request.full_url, code)
    response.msg = "Synthetic provider response"
    return response


class FakeSteam:
    def __init__(self, license_response=None, *, verified=True, response_code=200, response_headers=None):
        self.license_response = {"ownsapp": True} if license_response is None else license_response
        self.verified = verified
        self.response_code = response_code
        self.response_headers = response_headers
        self.requests = []

    def open(self, request, *, timeout):
        self.requests.append(request)
        endpoint = request.full_url.split("?", 1)[0]
        if endpoint == sync.OPENID_PROVIDER:
            verdict = "true" if self.verified else "false"
            return http_reply(request, f"ns:{sync.OPENID_NS}\nis_valid:{verdict}\n".encode("ascii"))
        if endpoint != PARTNER_ENDPOINT:
            raise AssertionError("Unexpected Steam endpoint")
        if isinstance(self.license_response, Exception):
            raise self.license_response
        body = self.license_response
        if not isinstance(body, bytes):
            body = json.dumps(body).encode("utf-8")
        return http_reply(request, body, self.response_code, self.response_headers)


class SteamEntitlementTests(unittest.TestCase):
    def check(self, provider, browser=None, *, expected_steam_id=None):
        if browser is None:
            browser = LocalBrowser()
        self.addCleanup(browser.join)
        status = entitlement.check_entitlement(
            APP_ID,
            publisher_app_id=APP_ID,
            publisher_key=PUBLISHER_KEY,
            expected_steam_id=expected_steam_id,
            timeout=3,
            opener=provider,
            open_browser=browser.open,
        )
        browser.join()
        return status

    def test_uses_only_the_verified_identity_and_exact_publisher_endpoint(self):
        # The Family Sharing owner can differ from the verified account.
        provider = FakeSteam({"appownership": {
            "ownsapp": True,
            "permanent": False,
            "ownersteamid": OTHER_ACCOUNT,
            "appid": APP_ID,
            "steamid": ACCOUNT,
        }})
        browser = LocalBrowser(wrong_path=True)
        self.assertEqual(self.check(provider, browser), Status.ACTIVE_LICENSE)
        self.assertEqual([reply[0] for reply in browser.replies], [404, 200])
        self.assertNotIn(PUBLISHER_KEY, browser.start_url)
        self.assertNotIn(ACCOUNT.encode("ascii"), browser.replies[-1][1])
        self.assertNotIn(PUBLISHER_KEY.encode("ascii"), browser.replies[-1][1])

        verify, lookup = provider.requests
        self.assertEqual(verify.full_url, sync.OPENID_PROVIDER)
        self.assertEqual(verify.get_method(), "POST")
        self.assertEqual(parse_qs(verify.data.decode("ascii"))["openid.mode"], ["check_authentication"])
        self.assertNotIn(PUBLISHER_KEY.encode("ascii"), verify.data)
        self.assertNotIn("x-webapi-key", {name.lower() for name, _ in verify.header_items()})

        target = urlsplit(lookup.full_url)
        self.assertEqual(lookup.full_url.split("?", 1)[0], PARTNER_ENDPOINT)
        self.assertEqual(lookup.get_method(), "GET")
        self.assertIsNone(lookup.data)
        self.assertEqual(parse_qs(target.query), {"appid": [str(APP_ID)], "steamid": [ACCOUNT], "format": ["json"]})
        self.assertNotIn(PUBLISHER_KEY, lookup.full_url)
        self.assertEqual({name.lower(): value for name, value in lookup.header_items()}["x-webapi-key"],
                         PUBLISHER_KEY)

    def test_scope_and_key_are_required_before_the_browser_or_network(self):
        cases = (
            (OTHER_APP_ID, PUBLISHER_KEY, Status.UNSUPPORTED_APP),
            (None, PUBLISHER_KEY, Status.PUBLISHER_NOT_CONFIGURED),
            (APP_ID, None, Status.PUBLISHER_NOT_CONFIGURED),
            (APP_ID, "", Status.PUBLISHER_NOT_CONFIGURED),
            (APP_ID, "g" * 32, Status.PUBLISHER_NOT_CONFIGURED),
            (APP_ID, PUBLISHER_KEY + "\r\n", Status.PUBLISHER_NOT_CONFIGURED),
        )
        for publisher_app_id, publisher_key, expected_status in cases:
            with self.subTest(publisher_app_id=publisher_app_id, status=expected_status):
                provider = FakeSteam()
                browser = LocalBrowser()
                status = entitlement.check_entitlement(
                    APP_ID,
                    publisher_app_id=publisher_app_id,
                    publisher_key=publisher_key,
                    timeout=3,
                    opener=provider,
                    open_browser=browser.open,
                )
                browser.join()
                self.assertEqual(status, expected_status)
                self.assertIsNone(browser.start_url)
                self.assertEqual(provider.requests, [])

    def test_unverified_or_mismatched_sign_in_cannot_query_ownership(self):
        provider = FakeSteam(verified=False)
        self.assertEqual(self.check(provider), Status.SIGN_IN_UNAVAILABLE)
        self.assertEqual([request.full_url for request in provider.requests], [sync.OPENID_PROVIDER])

        def mismatched_identity(return_to):
            return {**assertion(return_to), "openid.identity": f"https://steamcommunity.com/openid/id/{OTHER_ACCOUNT}"}

        rejected = FakeSteam()
        self.assertEqual(self.check(rejected, LocalBrowser(mismatched_identity)), Status.SIGN_IN_UNAVAILABLE)
        self.assertEqual(rejected.requests, [])

    def test_accepts_only_definite_boolean_ownership_in_either_supported_shape(self):
        for body, expected_status in (
            ({"ownsapp": True}, Status.ACTIVE_LICENSE),
            ({"ownsapp": False}, Status.NO_ACTIVE_LICENSE),
            ({"appownership": {"ownsapp": True}}, Status.ACTIVE_LICENSE),
            ({"appownership": {"ownsapp": False}}, Status.NO_ACTIVE_LICENSE),
        ):
            with self.subTest(body=body):
                self.assertEqual(self.check(FakeSteam(body)), expected_status)

    def test_malformed_ambiguous_and_wrongly_bound_results_are_unavailable(self):
        for body in (
            b"not JSON",
            b"[]",
            b"{}",
            b'{"ownsapp": true, "ownsapp": false}',
            b'{"appownership": {"ownsapp": false, "ownsapp": true}}',
            b'{"appownership": {"ownsapp": true}, "appownership": {"ownsapp": true}}',
            {"ownsapp": True, "appownership": {"ownsapp": True}},
            {"appownership": {"ownsapp": True}, "unexpected": 1},
            {"response": {"ownsapp": True}},
            {"appownership": {"response": {"ownsapp": True}}},
            {"appownership": {"ownsapp": True, "response": {"ownsapp": False}}},
            {"appownership": []},
            {"ownsapp": 1},
            {"ownsapp": "true"},
            {"ownsapp": None},
            {"ownsapp": True, "usercanceled": True},
            {"ownsapp": True, "steamid": OTHER_ACCOUNT},
            {"appownership": {"ownsapp": True, "appid": OTHER_APP_ID}},
        ):
            with self.subTest(body=body):
                self.assertEqual(self.check(FakeSteam(body)), Status.OWNERSHIP_UNAVAILABLE)

    def test_api_rejection_rate_limit_and_timeout_are_not_license_denials(self):
        for code in (403, 429, 500):
            with self.subTest(code=code):
                error = HTTPError(PARTNER_ENDPOINT, code, "synthetic provider error", Message(), BytesIO(b"error body"))
                steam = FakeSteam(error)
                self.assertEqual(self.check(steam), Status.OWNERSHIP_UNAVAILABLE)
                self.assertEqual(len(steam.requests), 2)
        self.assertEqual(self.check(FakeSteam(TimeoutError("synthetic timeout"))), Status.OWNERSHIP_UNAVAILABLE)

    def test_a_valid_license_body_requires_http_200(self):
        for code in (202, 206):
            with self.subTest(code=code):
                steam = FakeSteam({"ownsapp": True}, response_code=code)
                self.assertEqual(self.check(steam), Status.OWNERSHIP_UNAVAILABLE)
                self.assertEqual(len(steam.requests), 2)

    def test_http_200_requires_unambiguous_and_complete_body_framing(self):
        # The returned bytes are valid JSON even when HTTP does not frame them as a complete reply.
        body = b'{"ownsapp": true}'
        size = str(len(body))
        cases = (
            (),
            (("Content-Length", str(len(body) + 1)),),
            (("Content-Length", str(len(body) - 1)),),
            (("Content-Length", size), ("content-length", size)),
            (("Content-Length", "-1"),),
            (("Content-Length", "invalid"),),
            (("Content-Length", size + ", " + size),),
            (("Content-Length", str(entitlement.MAX_OWNERSHIP_BYTES + 1)),),
            (("Content-Length", size), ("Transfer-Encoding", "chunked")),
            (("Transfer-Encoding", "chunked"),),
            (("Transfer-Encoding", "gzip"),),
            (("Transfer-Encoding", "chunked"), ("transfer-encoding", "gzip")),
        )
        for fields in cases:
            with self.subTest(fields=fields):
                self.assertEqual(self.check(FakeSteam(body, response_headers=fields)), Status.OWNERSHIP_UNAVAILABLE)

        valid = FakeSteam(body, response_headers=(("Content-Length", size),))
        self.assertEqual(self.check(valid), Status.ACTIVE_LICENSE)

    def test_loopback_response_rejects_premature_connection_close(self):
        body = b'{"ownsapp": true}'

        class OwnershipHandler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                length = len(body) + (1 if self.path == "/truncated" else 0)
                self.send_header("Content-Length", str(length))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *args):
                pass

        with HTTPServer(("127.0.0.1", 0), OwnershipHandler) as server:
            worker = Thread(target=server.serve_forever, kwargs={"poll_interval": 0.02}, daemon=True)
            worker.start()
            try:
                url = f"http://127.0.0.1:{server.server_port}"
                opener = build_opener(ProxyHandler({}))
                self.assertEqual(entitlement._read_ownership_reply(opener, Request(url + "/complete")), body)
                self.assertIsNone(entitlement._read_ownership_reply(opener, Request(url + "/truncated")))
            finally:
                server.shutdown()
                worker.join()

    def test_default_steam_opener_ignores_ambient_proxies_and_rejects_redirects(self):
        proxy_env = {
            "http_proxy": "http://proxy.invalid:18990",
            "https_proxy": "http://proxy.invalid:18991",
            "all_proxy": "http://proxy.invalid:18992",
            "no_proxy": "",
        }
        proxy_env.update({name.upper(): value for name, value in proxy_env.items()})
        for location, expected in (
            (None, Status.ACTIVE_LICENSE),
            ("https://elsewhere.invalid/", Status.OWNERSHIP_UNAVAILABLE),
            ("https://[invalid/", Status.OWNERSHIP_UNAVAILABLE),
        ):
            with self.subTest(location=location):
                requests = []

                def synthetic_https(handler, request):
                    endpoint = request.full_url.split("?", 1)[0]
                    requests.append(endpoint)
                    self.assertEqual(request.host, urlsplit(endpoint).netloc)
                    if endpoint == sync.OPENID_PROVIDER:
                        data = f"ns:{sync.OPENID_NS}\nis_valid:true\n".encode("ascii")
                        return http_reply(request, data)
                    if endpoint != PARTNER_ENDPOINT:
                        raise AssertionError("The publisher request followed an unexpected redirect")
                    if location is not None:
                        response = http_reply(request, b"", 302, (("Location", location),))
                        # urllib can reject Location before the caller receives the response.
                        self.addCleanup(response.close)
                        return response
                    return http_reply(request, b'{"ownsapp": true}')

                with patch.dict(os.environ, proxy_env), patch.object(HTTPSHandler, "https_open", synthetic_https):
                    self.assertEqual(self.check(None), expected)
                self.assertEqual(requests, [sync.OPENID_PROVIDER, PARTNER_ENDPOINT])

    def test_tls_initialization_failure_stays_unverified(self):
        with patch.object(entitlement, "build_opener", side_effect=SSLError("Synthetic TLS setup error")), \
                patch.object(entitlement, "sign_in") as sign_in:
            self.assertEqual(entitlement.check_entitlement(
                APP_ID, publisher_app_id=APP_ID, publisher_key=PUBLISHER_KEY), Status.SIGN_IN_UNAVAILABLE)
            sign_in.assert_not_called()

    def invoke_cli(self, args):
        stdout = StringIO()
        stderr = StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            try:
                exit_code = entitlement.main(args)
            except SystemExit as error:
                exit_code = error.code
        return exit_code, stdout.getvalue(), stderr.getvalue()

    def test_cli_emits_one_credential_free_status_and_the_documented_exit_code(self):
        env = {SCOPE_ENV: str(APP_ID), KEY_ENV: PUBLISHER_KEY}
        definitive = (Status.ACTIVE_LICENSE, Status.NO_ACTIVE_LICENSE)
        for status in Status:
            with self.subTest(status=status), patch.dict(os.environ, env, clear=True), patch.object(
                    entitlement, "check_entitlement", return_value=status) as checked:
                exit_code, stdout, stderr = self.invoke_cli(["--app-id", str(APP_ID), "--timeout", "3"])
                self.assertEqual(exit_code, 0 if status in definitive else 1)
                self.assertEqual(json.loads(stdout), {"appid": APP_ID, "status": status.value})
                self.assertEqual(len(stdout.splitlines()), 1)
                self.assertNotIn(PUBLISHER_KEY, stdout + stderr)
                self.assertNotIn(ACCOUNT, stdout + stderr)
                checked.assert_called_once()

    def test_cli_browser_cannot_inherit_publisher_credentials(self):
        provider = FakeSteam()
        browser = LocalBrowser()
        self.addCleanup(browser.join)

        def guarded_browser(url):
            self.assertNotIn(KEY_ENV, os.environ)
            self.assertNotIn(sync.KEY_ENV, os.environ)
            return browser.open(url)

        env = {SCOPE_ENV: str(APP_ID), KEY_ENV: PUBLISHER_KEY, sync.KEY_ENV: "f" * 32}
        with patch.dict(os.environ, env, clear=True), patch.object(entitlement, "build_opener", return_value=provider), \
                patch.object(entitlement.webbrowser, "open", side_effect=guarded_browser):
            exit_code, stdout, stderr = self.invoke_cli(["--app-id", str(APP_ID), "--timeout", "3"])
            browser.join()
        self.assertEqual(exit_code, 0)
        self.assertEqual(json.loads(stdout), {"appid": APP_ID, "status": Status.ACTIVE_LICENSE.value})
        self.assertNotIn(PUBLISHER_KEY, stdout + stderr)
        self.assertEqual(len(provider.requests), 2)

    def test_cli_rejects_noncanonical_appids_timeouts_and_caller_supplied_identity(self):
        app_ids = ("0", "01", "+730", "-1", "4294967296", "٧٣٠", "73.0")
        bad_args = (["--app-id", value] for value in app_ids)
        with patch.object(entitlement, "check_entitlement") as checked:
            for args in [[], *bad_args, ["--app-id", str(APP_ID), "--timeout", "0"],
                    ["--app-id", str(APP_ID), "--timeout", "901"],
                    ["--app-id", str(APP_ID), "--steam-id", ACCOUNT]]:
                with self.subTest(args=args):
                    exit_code, stdout, stderr = self.invoke_cli(args)
                    self.assertEqual(exit_code, 2)
                    self.assertEqual(stdout, "")
                    self.assertNotIn(ACCOUNT, stderr)
            checked.assert_not_called()

    def test_matching_expected_account_checks_active_and_inactive_licenses(self):
        for owns_app, status in ((True, Status.ACTIVE_LICENSE), (False, Status.NO_ACTIVE_LICENSE)):
            with self.subTest(owns_app=owns_app):
                provider = FakeSteam({"ownsapp": owns_app})
                self.assertEqual(self.check(provider, expected_steam_id=ACCOUNT), status)
                verify, lookup = provider.requests
                self.assertEqual(verify.full_url, sync.OPENID_PROVIDER)
                self.assertEqual(parse_qs(urlsplit(lookup.full_url).query)["steamid"], [ACCOUNT])

    def test_cli_reports_account_mismatch_after_verification_without_requesting_ownership(self):
        def different_account(return_to):
            identity = f"https://steamcommunity.com/openid/id/{OTHER_ACCOUNT}"
            return {**assertion(return_to), "openid.claimed_id": identity, "openid.identity": identity}

        provider = FakeSteam()
        browser = LocalBrowser(different_account)
        self.addCleanup(browser.join)
        env = {SCOPE_ENV: str(APP_ID), KEY_ENV: PUBLISHER_KEY}
        with patch.dict(os.environ, env, clear=True), patch.object(entitlement, "build_opener", return_value=provider), \
                patch.object(entitlement.webbrowser, "open", side_effect=browser.open):
            exit_code, stdout, stderr = self.invoke_cli([
                "--app-id", str(APP_ID), "--expect-steam-id", ACCOUNT, "--timeout", "3",
            ])
            browser.join()

        self.assertEqual(exit_code, 1)
        self.assertEqual(json.loads(stdout), {"appid": APP_ID, "status": "account_mismatch"})
        self.assertEqual(stderr, "")
        self.assertNotIn(ACCOUNT, stdout + stderr)
        self.assertNotIn(OTHER_ACCOUNT, stdout + stderr)
        self.assertEqual([reply[0] for reply in browser.replies], [200])
        self.assertEqual([request.full_url for request in provider.requests], [sync.OPENID_PROVIDER])

    def test_invalid_expected_accounts_are_rejected_before_browser_or_network(self):
        for expected in (False, 0, -1, 1 << 64, "", "0" + ACCOUNT, "+" + ACCOUNT, "٧٣٠"):
            with self.subTest(expected=expected):
                provider = FakeSteam()
                browser = LocalBrowser()
                self.assertEqual(self.check(provider, browser, expected_steam_id=expected), Status.SIGN_IN_UNAVAILABLE)
                self.assertIsNone(browser.start_url)
                self.assertEqual(provider.requests, [])

        with patch.object(entitlement, "check_entitlement") as checked:
            for expected in ("0", "0" + ACCOUNT, "+" + ACCOUNT, str(1 << 64), "٧٣٠"):
                with self.subTest(expected=expected):
                    exit_code, stdout, stderr = self.invoke_cli([
                        "--app-id", str(APP_ID), "--expect-steam-id", expected,
                    ])
                    self.assertEqual(exit_code, 2)
                    self.assertEqual(stdout, "")
                    self.assertIn("--expect-steam-id must be a canonical positive uint64", stderr)
                    self.assertNotIn(ACCOUNT, stderr)
            checked.assert_not_called()


if __name__ == "__main__":
    unittest.main()
