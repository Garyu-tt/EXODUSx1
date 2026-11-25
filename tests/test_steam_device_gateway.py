from contextlib import closing
import hashlib
from http.client import HTTPConnection
import json
from threading import Thread
import unittest
from urllib.parse import parse_qs, urlencode, urlsplit

from exodus import steam_device_gateway as gateway
from exodus.steam_catalog import MAX_CATALOG_ENTRIES, steam_entry
from tests.test_steam_sync import ACCOUNT, API_KEY, FakeSteam, PlayedFreeSteam, assertion


ORIGIN = "https://exodus.example.org:8443"
VERIFIER = "2a" * 32
CHALLENGE = hashlib.sha256(bytes.fromhex(VERIFIER)).hexdigest()


class DeviceGatewayTests(unittest.TestCase):
    def setUp(self):
        self.time = 100
        self.provider = FakeSteam()
        self.server = gateway.DeviceGateway(
            ORIGIN, 0, API_KEY, opener=self.provider, clock=lambda: self.time,
        )
        self.addCleanup(self.server.server_close)
        worker = Thread(target=self.server.serve_forever, kwargs={"poll_interval": 0.02}, daemon=True)
        worker.start()
        self.addCleanup(worker.join, 5)
        self.addCleanup(self.server.shutdown)

    def request(self, method, target, body=None, *, headers=None):
        with closing(HTTPConnection("127.0.0.1", self.server.server_port, timeout=4)) as connection:
            payload = json.dumps(body).encode("ascii") if type(body) is dict else body
            request_headers = dict(headers or {})
            if payload is not None:
                request_headers.setdefault("Content-Type", "application/json")
            connection.request(method, target, body=payload, headers=request_headers)
            response = connection.getresponse()
            return response.status, response.headers, response.read()

    def create(self, challenge=CHALLENGE):
        status, headers, body = self.request("POST", gateway.SESSION_PATH, {"challenge": challenge})
        self.assertEqual(status, 201)
        self.assertEqual(headers["Cache-Control"], "no-store")
        response = json.loads(body)
        self.assertEqual(set(response), {"session"})
        self.assertRegex(response["session"], r"^[0-9a-f]{64}$")
        return response["session"]

    def redirect(self, session_id):
        status, headers, body = self.request("GET", f"/v1/steam/login/{session_id}")
        self.assertEqual((status, body), (302, b""))
        self.assertEqual(headers["Referrer-Policy"], "no-referrer")
        self.assertEqual(headers["Cache-Control"], "no-store")
        provider, query = headers["Location"].split("?", 1)
        self.assertEqual(provider, gateway.OPENID_PROVIDER)
        self.assertNotIn(API_KEY, query)
        fields = parse_qs(query)
        self.assertEqual(fields["openid.realm"], [ORIGIN + "/"])
        self.assertEqual(fields["openid.mode"], ["checkid_setup"])
        self.assertEqual(fields["openid.identity"], [gateway.OPENID_NS + "/identifier_select"])
        target = fields["openid.return_to"][0]
        self.assertTrue(target.startswith(ORIGIN + gateway.CALLBACK_PREFIX))
        self.assertRegex(urlsplit(target).path, r"^/v1/steam/callback/[0-9a-f]{64}$")
        return target

    def callback(self, return_to, fields):
        return self.request("GET", urlsplit(return_to).path + "?" + urlencode(fields))

    def redeem(self, session_id, verifier=VERIFIER):
        return self.request("POST", gateway.CATALOG_PATH, {
            "session": session_id,
            "verifier": verifier,
        })

    def cancel(self, session_id, verifier=VERIFIER):
        return self.request("POST", "/v1/steam/cancel", {
            "session": session_id,
            "verifier": verifier,
        })

    def assert_device_error(self, response, status, code):
        actual_status, headers, body = response
        self.assertEqual(actual_status, status)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(json.loads(body), {"error": code})

    def test_browser_sign_in_and_one_time_catalog_exchange(self):
        session_id = self.create()
        self.assert_device_error(self.redeem(session_id), 409, "sign_in_pending")
        self.assertEqual(self.provider.requests, [])
        return_to = self.redirect(session_id)
        self.assertEqual(self.redirect(session_id), return_to)
        fields = assertion(return_to)
        status, headers, body = self.callback(return_to, fields)
        self.assertEqual((status, headers["Location"], body), (303, ORIGIN + gateway.DONE_PATH, b""))
        self.assertEqual(self.request("GET", gateway.DONE_PATH)[0], 200)
        self.assertEqual(self.callback(return_to, fields)[0], 404)

        self.assert_device_error(self.redeem(session_id, "0a" * 32), 403, "invalid_proof")
        status, headers, body = self.redeem(session_id)
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Type"], "application/json")
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(json.loads(body), {"schema_version": 1, "entries": [
            steam_entry("10", "\u661f Test"),
            steam_entry("440", "Synthetic Team"),
        ]})
        self.assertNotIn(ACCOUNT.encode(), body)
        self.assertNotIn(API_KEY.encode(), body)
        self.assertNotIn(fields["openid.sig"].encode(), body)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")

        verify, games = self.provider.requests
        self.assertEqual(verify.get_method(), "POST")
        self.assertEqual(verify.full_url, gateway.OPENID_PROVIDER)
        self.assertEqual(parse_qs(verify.data.decode("ascii"))["openid.return_to"], [return_to])
        self.assertEqual(urlsplit(games.full_url).path, "/IPlayerService/GetOwnedGames/v1/")

    def test_callback_for_a_different_session_never_reaches_valve(self):
        previous_session = self.create()
        previous_return = self.redirect(previous_session)
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assertNotEqual(return_to, previous_return)
        self.assertEqual(self.callback(return_to, assertion(previous_return))[0], 400)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")
        self.assert_device_error(self.redeem(previous_session), 409, "sign_in_pending")
        self.assertEqual(self.provider.requests, [])

    def test_catalog_exchange_includes_played_free_games_in_mixed_and_free_only_libraries(self):
        for paid in (True, False):
            with self.subTest(paid=paid):
                self.provider = PlayedFreeSteam(paid=paid)
                self.server.opener = self.provider
                session_id = self.create()
                return_to = self.redirect(session_id)
                self.assertEqual(self.callback(return_to, assertion(return_to))[0], 303)
                status, headers, body = self.redeem(session_id)
                self.assertEqual((status, headers["Content-Type"]), (200, "application/json"))
                expected = ([steam_entry("10", "Purchased game")] if paid else [])
                expected.append(steam_entry("440", "Бесплатная игра"))
                self.assertEqual(json.loads(body), {"schema_version": 1, "entries": expected})
                self.assertNotIn(ACCOUNT.encode(), body)
                self.assertNotIn(API_KEY.encode(), body)
                self.assertEqual(len(self.provider.requests), 2)
                self.assert_device_error(self.redeem(session_id), 404, "session_not_found")

    def test_played_free_games_count_toward_the_catalog_limit(self):
        self.server.opener = PlayedFreeSteam(free_count=MAX_CATALOG_ENTRIES)
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assertEqual(self.callback(return_to, assertion(return_to))[0], 502)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")

    def test_device_cancellation_requires_proof_and_invalidates_the_browser_callback(self):
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assert_device_error(self.cancel(session_id, "0a" * 32), 403, "invalid_proof")
        self.assert_device_error(self.redeem(session_id), 409, "sign_in_pending")
        self.assertEqual(self.redirect(session_id), return_to)

        status, headers, body = self.cancel(session_id)
        self.assertEqual((status, body), (200, b""))
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assert_device_error(self.cancel(session_id), 404, "session_not_found")
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")
        self.assertEqual(self.request("GET", f"/v1/steam/login/{session_id}")[0], 404)
        self.assertEqual(self.callback(return_to, assertion(return_to))[0], 404)
        self.assertEqual(self.provider.requests, [])

    def test_device_can_discard_a_ready_catalog_after_closing_the_browser(self):
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assertEqual(self.callback(return_to, assertion(return_to))[0], 303)
        self.assert_device_error(self.cancel(session_id, "0a" * 32), 403, "invalid_proof")
        self.assertIsNotNone(self.server.sessions[session_id].catalog)
        status, _, body = self.cancel(session_id)
        self.assertEqual((status, body), (200, b""))
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")
        self.assertEqual(len(self.provider.requests), 2)

    def test_cancelled_attempts_reuse_session_capacity_without_waiting_for_expiry(self):
        sessions = [self.create() for _ in range(gateway.MAX_SESSIONS)]
        self.assert_device_error(
            self.request("POST", gateway.SESSION_PATH, {"challenge": CHALLENGE}),
            503, "sessions_full",
        )
        for _ in range(gateway.MAX_SESSIONS + 2):
            cancelled = sessions.pop(0)
            self.assertEqual(self.cancel(cancelled)[0], 200)
            sessions.append(self.create())
        self.assertEqual(len(self.server.sessions), gateway.MAX_SESSIONS)
        self.assertEqual(self.time, 100)
        self.assertEqual(self.provider.requests, [])

    def test_cancellation_rejects_expired_and_malformed_requests(self):
        session_id = self.create()
        for body in (
            {"session": session_id},
            {"session": session_id, "verifier": VERIFIER, "extra": "value"},
            {"session": session_id, "verifier": VERIFIER.upper()},
            b'{"session":"1","session":"2"}',
        ):
            with self.subTest(body=body):
                self.assert_device_error(
                    self.request("POST", "/v1/steam/cancel", body), 400, "invalid_request",
                )
        self.assert_device_error(self.redeem(session_id), 409, "sign_in_pending")
        self.time += gateway.SESSION_LIFETIME
        self.assert_device_error(self.cancel(session_id), 404, "session_not_found")
        self.assertEqual(self.provider.requests, [])

    def test_cancellation_and_unverified_assertion_discard_the_session(self):
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assertEqual(self.callback(return_to, {"openid.mode": "cancel"})[0], 400)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")
        self.assertEqual(self.provider.requests, [])

        self.provider.valid = False
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assertEqual(self.callback(return_to, assertion(return_to))[0], 400)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")
        self.assertEqual(len(self.provider.requests), 1)

    def test_unavailable_library_does_not_create_an_empty_catalog(self):
        self.provider.library = {"response": {}}
        session_id = self.create()
        return_to = self.redirect(session_id)
        status, headers, body = self.callback(return_to, assertion(return_to))
        self.assertEqual(status, 502)
        self.assertIsNone(headers["Location"])
        self.assertNotIn(ACCOUNT.encode(), body)
        self.assertNotIn(API_KEY.encode(), body)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")

    def test_expired_session_and_callback_release_capacity(self):
        session_ids = [self.create() for _ in range(gateway.MAX_SESSIONS)]
        return_to = self.redirect(session_ids[0])
        self.assert_device_error(
            self.request("POST", gateway.SESSION_PATH, {"challenge": CHALLENGE}),
            503, "sessions_full",
        )
        self.time += gateway.SESSION_LIFETIME
        self.assertEqual(self.callback(return_to, assertion(return_to))[0], 404)
        self.assert_device_error(self.redeem(session_ids[0]), 404, "session_not_found")
        self.assertNotIn(self.create(), session_ids)
        self.assertEqual(self.provider.requests, [])

    def test_library_that_becomes_ready_after_expiry_is_discarded(self):
        opener = self.provider.open

        def finish_after_expiry(request, *, timeout):
            response = opener(request, timeout=timeout)
            if urlsplit(request.full_url).path == "/IPlayerService/GetOwnedGames/v1/":
                self.time += gateway.SESSION_LIFETIME
            return response

        self.provider.open = finish_after_expiry
        session_id = self.create()
        return_to = self.redirect(session_id)
        self.assertEqual(self.callback(return_to, assertion(return_to))[0], 404)
        self.assert_device_error(self.redeem(session_id), 404, "session_not_found")

    def test_invalid_device_requests_are_rejected_before_steam(self):
        for body in (
            {"challenge": CHALLENGE, "origin": "https://other.example.org"},
            {"challenge": CHALLENGE.upper()},
            {"challenge": 12},
            b"[]",
            b'{"challenge":"1","challenge":"2"}',
        ):
            with self.subTest(body=body[:20] if type(body) is bytes else body):
                self.assert_device_error(
                    self.request("POST", gateway.SESSION_PATH, body), 400, "invalid_request",
                )
        self.assertEqual(self.request("GET", "/v1/steam/done?next=https://other.example.org")[0], 404)
        self.assertEqual(self.provider.requests, [])

    def test_invalid_device_headers_are_rejected_without_a_body(self):
        # These cases must get a response before any body is sent.
        for content_type, length in (
            ("application/json", gateway.MAX_REQUEST_BODY + 1),
            ("text/plain", 1),
        ):
            with self.subTest(content_type=content_type, content_length=length):
                response = self.request("POST", gateway.SESSION_PATH, headers={
                    "Content-Type": content_type, "Content-Length": str(length),
                })
                self.assert_device_error(response, 400, "invalid_request")
        self.assertEqual(self.provider.requests, [])


class OriginTests(unittest.TestCase):
    def test_uses_canonical_https_origin(self):
        self.assertEqual(gateway.https_origin("https://EXODUS.example.org:443/"),
                         "https://exodus.example.org")
        self.assertEqual(gateway.https_origin(ORIGIN), ORIGIN)

    def test_rejects_url_components_other_than_https_authority(self):
        for value in (
            "http://exodus.example.org",
            "https://exodus.example.org/steam",
            "https://exodus.example.org/?next=https://other.example.org",
            "https://exodus.example.org/#rest",
            "https://user@exodus.example.org",
            "https://exodus.example.org\n",
            "https://exodus.example.org:0",
            "https://exodus.example.org:65536",
            "https://exodus..example.org",
        ):
            with self.subTest(value=value), self.assertRaises(ValueError):
                gateway.https_origin(value)


if __name__ == "__main__":
    unittest.main()
