from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
import os
import sys
import unittest
from unittest.mock import patch
from urllib.parse import parse_qs, urlsplit

from exodus import steam_entitlement as entitlement
from exodus import steam_entitlement_acceptance as acceptance
from exodus import steam_sync as sync
from tests.test_steam_entitlement import (
    ACCOUNT,
    APP_ID,
    FakeSteam,
    KEY_ENV,
    OTHER_ACCOUNT,
    PARTNER_ENDPOINT,
    PUBLISHER_KEY,
    SCOPE_ENV,
)
from tests.test_steam_sync import LocalBrowser, assertion


LIBRARY_KEY = "f" * 32
CONFIG = {SCOPE_ENV: str(APP_ID), KEY_ENV: PUBLISHER_KEY, sync.KEY_ENV: LIBRARY_KEY}
CLI_ARGS = ["--app-id", str(APP_ID), "--licensed-steam-id", ACCOUNT, "--unlicensed-steam-id", OTHER_ACCOUNT]
GOOD_CASE = {"case": "licensed", "expected": "active_license", "observed": "active_license"}


class MatrixSteam(FakeSteam):
    def __init__(self, licenses):
        super().__init__()
        self.licenses = licenses
        self.publisher_accounts = []

    def open(self, request, *, timeout):
        target = urlsplit(request.full_url)
        if request.full_url.split("?", 1)[0] == PARTNER_ENDPOINT:
            fields = parse_qs(target.query)
            account = fields["steamid"][0]
            self.publisher_accounts.append(account)
            self.license_response = {
                "appid": int(fields["appid"][0]), "steamid": account, "ownsapp": self.licenses[account],
            }
        return super().open(request, timeout=timeout)


class SteamEntitlementAcceptanceTests(unittest.TestCase):
    def invoke(self, text, *, accounts=(), config=CONFIG, args=CLI_ARGS, licenses=None):
        if licenses is None:
            licenses = {ACCOUNT: True, OTHER_ACCOUNT: False}
        provider = MatrixSteam(licenses)
        remaining_accounts = iter(accounts)
        browsers = []
        stdout = StringIO()
        stderr = StringIO()

        def open_browser(url):
            self.assertNotIn(KEY_ENV, os.environ)
            self.assertNotIn(sync.KEY_ENV, os.environ)
            account = next(remaining_accounts)

            def signed_account(return_to):
                identity = f"https://steamcommunity.com/openid/id/{account}"
                return {**assertion(return_to), "openid.claimed_id": identity, "openid.identity": identity}

            browser = LocalBrowser(signed_account)
            browsers.append(browser)
            self.addCleanup(browser.join)
            return browser.open(url)

        with patch.dict(os.environ, config, clear=True), patch.object(sys, "stdin", StringIO(text)), \
                patch.object(entitlement, "build_opener", return_value=provider), \
                patch.object(entitlement.webbrowser, "open", side_effect=open_browser), \
                redirect_stdout(stdout), redirect_stderr(stderr):
            try:
                exit_code = acceptance.main(args)
            except SystemExit as error:
                exit_code = error.code
            for browser in browsers:
                browser.join()

        output = stdout.getvalue()
        prompts = stderr.getvalue()
        for private_value in (ACCOUNT, OTHER_ACCOUNT, PUBLISHER_KEY, LIBRARY_KEY):
            self.assertNotIn(private_value, output + prompts)
        if output:
            self.assertEqual(len(output.splitlines()), 1)
        return exit_code, json.loads(output) if output else None, prompts, provider, browsers

    def test_real_loopback_matrix_with_two_ownership_requests(self):
        result, report, prompts, provider, browsers = self.invoke("\n" * 3, accounts=(ACCOUNT, OTHER_ACCOUNT, OTHER_ACCOUNT))
        self.assertEqual(result, 0)
        self.assertEqual(report, {"appid": APP_ID, "status": "passed", "cases": [
            GOOD_CASE,
            {"case": "unlicensed", "expected": "no_active_license", "observed": "no_active_license"},
            {"case": "wrong_account", "expected": "account_mismatch", "observed": "account_mismatch"},
        ]})
        self.assertEqual(len(prompts.splitlines()), 3)
        self.assertEqual([browser.replies[0][0] for browser in browsers], [200, 200, 200])
        self.assertEqual(len({browser.start_url for browser in browsers}), 3)
        self.assertEqual(provider.publisher_accounts, [ACCOUNT, OTHER_ACCOUNT])
        self.assertEqual([request.full_url.split("?", 1)[0] for request in provider.requests], [
            sync.OPENID_PROVIDER, PARTNER_ENDPOINT, sync.OPENID_PROVIDER, PARTNER_ENDPOINT, sync.OPENID_PROVIDER,
        ])

    def test_first_unexpected_status_stops_remaining_sign_ins(self):
        result, report, prompts, provider, browsers = self.invoke("\n" * 3, accounts=(ACCOUNT,), licenses={ACCOUNT: False})
        self.assertEqual(result, 1)
        self.assertEqual(report, {"appid": APP_ID, "status": "failed", "cases": [
            {"case": "licensed", "expected": "active_license", "observed": "no_active_license"},
        ]})
        self.assertEqual(len(prompts.splitlines()), 1)
        self.assertEqual(len(browsers), 1)
        self.assertEqual(provider.publisher_accounts, [ACCOUNT])

    def test_eof_or_cancel_preserves_completed_cases(self):
        for text in ("\n", "\nq\n"):
            with self.subTest(text=text):
                result, report, prompts, provider, browsers = self.invoke(text, accounts=(ACCOUNT,))
                self.assertEqual(result, 1)
                self.assertEqual(report, {"appid": APP_ID, "status": "aborted", "cases": [GOOD_CASE]})
                self.assertEqual(len(prompts.splitlines()), 2)
                self.assertEqual(len(browsers), 1)
                self.assertEqual(provider.publisher_accounts, [ACCOUNT])

    def test_invalid_publisher_configuration_stops_before_prompts(self):
        for env, status in (
            ({}, "publisher_not_configured"),
            ({SCOPE_ENV: str(APP_ID)}, "publisher_not_configured"),
            ({**CONFIG, KEY_ENV: "invalid"}, "publisher_not_configured"),
            ({**CONFIG, SCOPE_ENV: str(APP_ID + 1)}, "unsupported_app"),
        ):
            with self.subTest(status=status, has_scope=SCOPE_ENV in env):
                result, report, prompts, provider, browsers = self.invoke("\n" * 3, config=env)
                self.assertEqual(result, 1)
                self.assertEqual(report, {"appid": APP_ID, "status": status, "cases": []})
                self.assertEqual(prompts, "")
                self.assertEqual(provider.requests, [])
                self.assertEqual(browsers, [])

    def test_invalid_usage_stops_before_any_sign_in(self):
        for args in (
            [*CLI_ARGS[:-1], ACCOUNT],
            [*CLI_ARGS[:-1], "0" + OTHER_ACCOUNT],
            [*CLI_ARGS[:-1], str(1 << 64)],
            [*CLI_ARGS, "--timeout", "0"],
            [*CLI_ARGS, "--timeout", "901"],
            [*CLI_ARGS, "--steam-id", OTHER_ACCOUNT],
        ):
            with self.subTest(args_count=len(args)):
                result, report, prompts, provider, browsers = self.invoke("\n" * 3, args=args)
                self.assertEqual(result, 2)
                self.assertIsNone(report)
                self.assertNotIn("Press Enter", prompts)
                self.assertEqual(provider.requests, [])
                self.assertEqual(browsers, [])


if __name__ == "__main__":
    unittest.main()
