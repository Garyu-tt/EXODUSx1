"""Serve the Steam sign-in and catalog exchange for EXODUS devices."""

import argparse
from dataclasses import dataclass
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
import re
import secrets
import sys
import time
from typing import Optional
from urllib.parse import urlencode, urlsplit
from urllib.request import build_opener

from .steam_sync import (
    KEY_ENV,
    MAX_CALLBACK_BYTES,
    MAX_CATALOG_BYTES,
    OPENID_NS,
    OPENID_PROVIDER,
    NoSteamRedirect,
    SteamSyncError,
    decode_json,
    fetch_library,
    parse_openid_response,
    verify_openid,
)


SESSION_PATH = "/v1/steam/sessions"
CATALOG_PATH = "/v1/steam/catalog"
CANCEL_PATH = "/v1/steam/cancel"
CALLBACK_PREFIX = "/v1/steam/callback/"
DONE_PATH = "/v1/steam/done"
TOKEN = re.compile(r"[0-9a-f]{64}", re.ASCII)
BROWSER_PATH = re.compile(r"/v1/steam/(login|callback)/([0-9a-f]{64})", re.ASCII)
ORIGIN = re.compile(r"https://[a-zA-Z0-9.-]+(?::[0-9]{1,5})?/?", re.ASCII)
DNS_LABEL = re.compile(r"[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?", re.ASCII)
SESSION_LIFETIME = 300
MAX_SESSIONS = 32
MAX_REQUEST_BODY = 1024


def https_origin(value):
    if len(value) > 267 or not ORIGIN.fullmatch(value):
        raise ValueError("Use an HTTPS origin with a hostname and optional port.")
    configured = urlsplit(value)
    host = configured.hostname
    labels = host.split(".")
    port = configured.port
    if (len(host) > 253 or len(labels) < 2
            or any(not DNS_LABEL.fullmatch(label) for label in labels)
            or port == 0):
        raise ValueError("Use a valid HTTPS hostname and port.")
    return f"https://{host}" + (f":{port}" if port not in (None, 443) else "")


@dataclass
class DeviceSession:
    challenge: str
    expires_at: float
    callback_state: Optional[str] = None
    catalog: Optional[bytes] = None


class DeviceGatewayHandler(BaseHTTPRequestHandler):
    timeout = 5

    def log_message(self, format, *args):
        # Request targets may contain OpenID assertions or one-time session tokens.
        pass

    def send_error(self, code, message=None, explain=None):
        self.json_error(code, "invalid_request")

    def reply(self, status, data=b"", *, content_type="text/plain; charset=utf-8", location=None):
        self.close_connection = True
        try:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("X-Content-Type-Options", "nosniff")
            if location is not None:
                self.send_header("Location", location)
            self.end_headers()
            if data:
                self.wfile.write(data)
        except OSError:
            pass

    def json_reply(self, status, payload):
        self.reply(status, json.dumps(payload).encode("ascii"), content_type="application/json")

    def json_error(self, status, code):
        self.json_reply(status, {"error": code})

    def request_target(self):
        if (len(self.path) > MAX_CALLBACK_BYTES or not self.path.startswith("/")
                or "#" in self.path or any(not 0x21 <= ord(ch) <= 0x7E for ch in self.path)):
            return None
        try:
            target = urlsplit(self.path)
        except ValueError:
            return None
        return target if not target.scheme and not target.netloc else None

    def read_device_request(self, fields):
        lengths = self.headers.get_all("Content-Length") or []
        if (self.headers.get_all("Transfer-Encoding") or len(lengths) != 1
                or re.fullmatch(r"[0-9]{1,4}", lengths[0]) is None
                or self.headers.get_content_type() != "application/json"):
            raise SteamSyncError("Invalid device request.")
        length = int(lengths[0])
        if not 0 < length <= MAX_REQUEST_BODY:
            raise SteamSyncError("Invalid device request.")
        body = self.rfile.read(length)
        if len(body) != length:
            raise SteamSyncError("Invalid device request.")
        document = decode_json(body, "the device request")
        if (type(document) is not dict or document.keys() != fields
                or any(type(token) is not str or TOKEN.fullmatch(token) is None
                       for token in document.values())):
            raise SteamSyncError("Invalid device request.")
        return document

    def do_POST(self):
        target = self.request_target()
        if target is None or target.query:
            self.json_error(400, "invalid_request")
            return
        if target.path not in (SESSION_PATH, CATALOG_PATH, CANCEL_PATH):
            self.json_error(404, "not_found")
            return
        fields = {"challenge"} if target.path == SESSION_PATH else {"session", "verifier"}
        try:
            request = self.read_device_request(fields)
        except (OSError, SteamSyncError):
            self.json_error(400, "invalid_request")
            return
        self.server.prune_expired()
        if target.path == SESSION_PATH:
            self.start_session(request["challenge"])
        elif target.path == CATALOG_PATH:
            self.redeem_catalog(request["session"], request["verifier"])
        else:
            self.cancel_session(request["session"], request["verifier"])

    def start_session(self, challenge):
        sessions = self.server.sessions
        if len(sessions) >= MAX_SESSIONS:
            self.json_error(503, "sessions_full")
            return
        session_id = secrets.token_hex(32)
        sessions[session_id] = DeviceSession(challenge, self.server.clock() + SESSION_LIFETIME)
        self.json_reply(201, {"session": session_id})

    def session_with_proof(self, session_id, verifier):
        session = self.server.sessions.get(session_id)
        if session is None:
            self.json_error(404, "session_not_found")
            return None
        proof = hashlib.sha256(bytes.fromhex(verifier)).hexdigest()
        if not secrets.compare_digest(proof, session.challenge):
            self.json_error(403, "invalid_proof")
            return None
        return session

    def redeem_catalog(self, session_id, verifier):
        session = self.session_with_proof(session_id, verifier)
        if session is None:
            return
        if session.catalog is None:
            self.json_error(409, "sign_in_pending")
        else:
            del self.server.sessions[session_id]
            self.reply(200, session.catalog, content_type="application/json")

    def cancel_session(self, session_id, verifier):
        if self.session_with_proof(session_id, verifier) is None:
            return
        del self.server.sessions[session_id]
        self.reply(200)

    def do_GET(self):
        target = self.request_target()
        if target is None:
            self.reply(400, b"Invalid request.\n")
            return
        self.server.prune_expired()
        if target.path == DONE_PATH and not target.query:
            self.reply(200, b"Return to EXODUS on your console.\n")
            return
        endpoint = BROWSER_PATH.fullmatch(target.path)
        if endpoint is None or (endpoint[1] == "login" and target.query):
            self.reply(404, b"Page not found.\n")
        elif endpoint[1] == "login":
            self.start_login(endpoint[2])
        else:
            self.finish_login(endpoint[2], target.query)

    def start_login(self, session_id):
        session = self.server.sessions.get(session_id)
        if session is None:
            self.reply(404, b"Open a new sign-in session in EXODUS.\n")
            return
        if session.catalog is not None:
            self.reply(303, location=self.server.origin + DONE_PATH)
            return
        if session.callback_state is None:
            session.callback_state = secrets.token_hex(32)
        identifier = OPENID_NS + "/identifier_select"
        query = urlencode({
            "openid.ns": OPENID_NS,
            "openid.mode": "checkid_setup",
            "openid.realm": self.server.origin + "/",
            "openid.return_to": self.server.origin + CALLBACK_PREFIX + session.callback_state,
            "openid.claimed_id": identifier,
            "openid.identity": identifier,
        })
        self.reply(302, location=OPENID_PROVIDER + "?" + query)

    def finish_login(self, state, query):
        sessions = self.server.sessions
        match = next(((key, session) for key, session in sessions.items()
                      if session.callback_state == state), None)
        if match is None:
            self.reply(404, b"Open a new sign-in session in EXODUS.\n")
            return
        session_id, session = match
        session.callback_state = None
        return_to = self.server.origin + CALLBACK_PREFIX + state
        try:
            fields = parse_openid_response(query)
            steam_id = verify_openid(fields, return_to, self.server.opener)
        except SteamSyncError:
            del sessions[session_id]
            self.reply(400, b"Steam sign-in did not complete. Return to EXODUS and try again.\n")
            return
        try:
            entries = fetch_library(self.server.opener, self.server.key, steam_id)
            catalog = json.dumps(
                {"schema_version": 1, "entries": entries}, ensure_ascii=False, separators=(",", ":"),
            ).encode("utf-8")
            if len(catalog) > MAX_CATALOG_BYTES:
                raise SteamSyncError("Steam catalog exceeds the HOME size limit.")
        except SteamSyncError:
            del sessions[session_id]
            self.reply(502, b"Could not prepare the Steam library. Check Web API access and retry.\n")
            return
        if session.expires_at <= self.server.clock():
            del sessions[session_id]
            self.reply(404, b"Open a new sign-in session in EXODUS.\n")
            return
        session.catalog = catalog
        self.reply(303, location=self.server.origin + DONE_PATH)


class DeviceGateway(HTTPServer):
    def __init__(self, origin, port, key, *, opener=None, clock=time.monotonic):
        self.origin = https_origin(origin)
        self.key = key
        self.opener = build_opener(NoSteamRedirect) if opener is None else opener
        self.clock = clock
        self.sessions = {}
        super().__init__(("127.0.0.1", port), DeviceGatewayHandler)

    def prune_expired(self):
        now = self.clock()
        for key, session in list(self.sessions.items()):
            if session.expires_at <= now:
                del self.sessions[key]

    def service_actions(self):
        self.prune_expired()

    def handle_error(self, request, client_address):
        print("A Steam gateway request failed.", file=sys.stderr)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--origin", required=True, help="Public HTTPS origin of the reverse proxy.")
    parser.add_argument("--port", type=int, default=8787, help="Loopback HTTP port (default: 8787).")
    args = parser.parse_args(argv)
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    key = os.environ.pop(KEY_ENV, None)
    if re.fullmatch(r"[0-9a-fA-F]{32}", key or "") is None:
        parser.error(f"set {KEY_ENV} to a Steam Web API key")
    try:
        gateway = DeviceGateway(args.origin, args.port, key)
    except ValueError as error:
        parser.error(str(error))
    except OSError:
        print("Could not start the Steam gateway on loopback.", file=sys.stderr)
        return 1
    with gateway:
        print(f"Steam gateway listening on loopback port {gateway.server_port}.", flush=True)
        try:
            gateway.serve_forever()
        except KeyboardInterrupt:
            return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
