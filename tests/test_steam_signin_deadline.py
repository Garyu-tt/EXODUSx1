from io import BytesIO
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from exodus import steam_entitlement as entitlement
from exodus import steam_sync as sync
from tests.test_steam_sync import ACCOUNT, API_KEY, FakeSteam, LocalBrowser, assertion


class TimedSteam(FakeSteam):
    """Advance a deterministic clock while the real callback is verifying."""

    def __init__(self, clock, finished_at, *, phase="read", fail=False):
        super().__init__()
        self.clock = clock
        self.finished_at = finished_at
        self.phase = phase
        self.fail = fail
        self.timeouts = []
        self.verification_response = None

    def open(self, request, *, timeout):
        self.timeouts.append(timeout)
        response = super().open(request, timeout=timeout)
        if request.full_url != sync.OPENID_PROVIDER:
            return response
        if self.phase == "open":
            self.clock[0] = self.finished_at
        owner = self

        class VerificationReply(BytesIO):
            def read(self, size=-1):
                if owner.phase == "read":
                    owner.clock[0] = owner.finished_at
                if owner.fail:
                    raise TimeoutError("synthetic provider timeout")
                return super().read(size)

        with response:
            self.verification_response = VerificationReply(response.read())
        return self.verification_response


class SteamSignInDeadlineTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.output = Path(directory.name) / "catalog.json"
        self.original = b"previous catalog retained on failed sign-in"
        self.output.write_bytes(self.original)
        self.clock = [0.0]
        self.browser = LocalBrowser()
        self.addCleanup(self.browser.join)
        self.time_patch = patch.object(sync.time, "monotonic", side_effect=lambda: self.clock[0])
        self.time_patch.start()
        self.addCleanup(self.time_patch.stop)

    def run_sync(self, provider):
        return sync.sync_library(self.output, API_KEY, timeout=5,
                                 open_browser=self.browser.open, opener=provider)

    def assert_rejected(self, provider, *, requests):
        self.browser.join()
        self.assertEqual(self.output.read_bytes(), self.original)
        self.assertEqual(len(provider.requests), requests)
        self.assertEqual([row[0] for row in self.browser.replies], [400])
        self.assertNotIn(ACCOUNT.encode(), self.browser.replies[0][1])
        self.assertNotIn(API_KEY.encode(), self.browser.replies[0][1])
        self.assertEqual(list(self.output.parent.iterdir()), [self.output])

    def test_late_verification_never_fetches_library_or_replaces_catalog(self):
        for phase in ("open", "read"):
            for finished_at in (5.0, 5.25):
                with self.subTest(phase=phase, finished_at=finished_at):
                    self.clock[0] = 0.0
                    self.browser = LocalBrowser()
                    self.addCleanup(self.browser.join)
                    provider = TimedSteam(self.clock, finished_at, phase=phase)
                    with self.assertRaisesRegex(sync.SteamSyncError, "Timed out"):
                        self.run_sync(provider)
                    self.assert_rejected(provider, requests=1)
                    self.assertTrue(provider.verification_response.closed)

    def test_callback_received_after_deadline_does_not_contact_steam(self):
        original = sync.SteamCallbackHandler.do_GET

        def late_callback(handler):
            self.clock[0] = 5.0
            original(handler)

        provider = TimedSteam(self.clock, 5.25)
        with patch.object(sync.SteamCallbackHandler, "do_GET", late_callback):
            with self.assertRaisesRegex(sync.SteamSyncError, "Timed out"):
                self.run_sync(provider)
        self.assert_rejected(provider, requests=0)

    def test_verification_request_uses_remaining_budget(self):
        original = sync.SteamCallbackHandler.do_GET

        def admitted_callback(handler):
            self.clock[0] = 2.5
            original(handler)

        provider = TimedSteam(self.clock, 4.75)
        with patch.object(sync.SteamCallbackHandler, "do_GET", admitted_callback):
            self.assertEqual(self.run_sync(provider), 2)
        self.browser.join()
        self.assertEqual(provider.timeouts, [2.5, 10])
        self.assertEqual([row[0] for row in self.browser.replies], [200])
        self.assertEqual(len(json.loads(self.output.read_bytes())["entries"]), 2)

    def test_completed_verification_survives_slow_browser_reply(self):
        original = sync.SteamCallbackHandler.reply

        def delayed_reply(handler, status, text):
            if status == 200:
                self.clock[0] = 5.25
            original(handler, status, text)

        provider = TimedSteam(self.clock, 4.75)
        with patch.object(sync.SteamCallbackHandler, "reply", delayed_reply):
            self.assertEqual(self.run_sync(provider), 2)
        self.browser.join()
        self.assertEqual([row[0] for row in self.browser.replies], [200])
        self.assertEqual(len(provider.requests), 2)

    def test_provider_timeout_at_deadline_preserves_catalog(self):
        provider = TimedSteam(self.clock, 5.0, fail=True)
        with self.assertRaisesRegex(sync.SteamSyncError, "Timed out"):
            self.run_sync(provider)
        self.assert_rejected(provider, requests=1)
        self.assertTrue(provider.verification_response.closed)

    def test_entitlement_does_not_use_identity_verified_after_deadline(self):
        provider = TimedSteam(self.clock, 5.25)
        result = entitlement.check_entitlement(
            440, publisher_app_id=440, publisher_key=API_KEY, expected_steam_id=ACCOUNT,
            timeout=5, open_browser=self.browser.open, opener=provider,
        )
        self.assertEqual(result, entitlement.LicenseStatus.SIGN_IN_UNAVAILABLE)
        self.assert_rejected(provider, requests=1)

    def test_direct_gateway_verification_keeps_default_request_timeout(self):
        provider = TimedSteam(self.clock, 100.0)
        return_to = "https://exodus.example.org/steam/callback/synthetic"
        self.assertEqual(sync.verify_openid(assertion(return_to), return_to, provider), ACCOUNT)
        self.assertEqual(provider.timeouts, [10])


if __name__ == "__main__":
    unittest.main()
