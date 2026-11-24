from contextlib import redirect_stderr, redirect_stdout
from email.message import Message
from io import BytesIO, StringIO
import json
import os
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlsplit
from urllib.request import HTTPSHandler
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


class FakeSteam:
    def __init__(self, license_response=None, *, verified=True):
        self.license_response = {"ownsapp": True} if license_response is None else license_response
        self.verified = verified
        self.requests = []

    def open(self, request, *, timeout):
        self.requests.append(request)
        endpoint = request.full_url.split("?", 1)[0]
        if endpoint == sync.OPENID_PROVIDER:
            verdict = "true" if self.verified else "false"
            return BytesIO(f"ns:{sync.OPENID_NS}\nis_valid:{verdict}\n".encode("ascii"))
        if endpoint != PARTNER_ENDPOINT:
            raise AssertionError("Unexpected Steam endpoint")
        if isinstance(self.license_response, Exception):
            raise self.license_response
        if isinstance(self.license_response, bytes):
            return BytesIO(self.license_response)
        return BytesIO(json.dumps(self.license_response).encode("utf-8"))


class SteamEntitlementTests(unittest.TestCase):
    def check(self, provider, browser=None):
        if browser is None:
            browser = LocalBrowser()
        self.addCleanup(browser.join)
        status = entitlement.check_entitlement(
            APP_ID,
            publisher_app_id=APP_ID,
            publisher_key=PUBLISHER_KEY,
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

    def test_default_steam_opener_does_not_follow_a_redirect_with_the_publisher_key(self):
        requests = []

        def synthetic_https(handler, request):
            endpoint = request.full_url.split("?", 1)[0]
            requests.append(endpoint)
            headers = Message()
            if endpoint == sync.OPENID_PROVIDER:
                data = f"ns:{sync.OPENID_NS}\nis_valid:true\n".encode("ascii")
                response = addinfourl(BytesIO(data), headers, request.full_url, 200)
                response.msg = "OK"
            elif endpoint == PARTNER_ENDPOINT:
                headers["Location"] = "https://elsewhere.invalid/"
                response = addinfourl(BytesIO(), headers, request.full_url, 302)
                response.msg = "Found"
            else:
                raise AssertionError("The publisher request followed an unexpected redirect")
            return response

        with patch.dict(os.environ, {}, clear=True), patch.object(HTTPSHandler, "https_open", synthetic_https):
            self.assertEqual(self.check(None), Status.OWNERSHIP_UNAVAILABLE)
        self.assertEqual(requests, [sync.OPENID_PROVIDER, PARTNER_ENDPOINT])

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
                    exit_code, stdout, _ = self.invoke_cli(args)
                    self.assertEqual(exit_code, 2)
                    self.assertEqual(stdout, "")
            checked.assert_not_called()


if __name__ == "__main__":
    unittest.main()
