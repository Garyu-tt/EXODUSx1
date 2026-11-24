"""Sign in through Steam in a desktop browser and export a HOME catalog."""

import argparse
from concurrent.futures import Future
from datetime import datetime, timezone
from http.client import HTTPException
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import re
import secrets
import sys
from threading import Thread
import time
from urllib.error import HTTPError
from urllib.parse import parse_qsl, urlencode, urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener
import webbrowser

from .steam_catalog import (
    CatalogLimitError,
    CatalogOutputError,
    MAX_CATALOG_BYTES,
    MAX_CATALOG_ENTRIES,
    steam_entry,
    write_catalog_file,
)


OPENID_PROVIDER = "https://steamcommunity.com/openid/login"
OPENID_NS = "http://specs.openid.net/auth/2.0"
OWNED_GAMES = "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/"
STEAM_ID = re.compile(r"https?://steamcommunity\.com/openid/id/([1-9][0-9]{0,19})")
KEY_ENV = "STEAM_WEB_API_KEY"
MAX_CALLBACK_BYTES = 8192
MAX_ASSERTION_AGE = 300
REQUIRED_SIGNATURE_FIELDS = {
    "op_endpoint", "claimed_id", "identity", "return_to", "response_nonce", "assoc_handle",
}
ENTRY_TEXT_FIELDS = {"provider", "provider_id", "name", "installation_state", "compatibility_state"}
ENTRY_OPTIONAL_TEXT_FIELDS = {"artwork", "launch_mode"}
ENTRY_FIELDS = ENTRY_TEXT_FIELDS | ENTRY_OPTIONAL_TEXT_FIELDS | {"last_played"}


class SteamSyncError(Exception):
    """A failure that can be reported without showing Steam credentials or account data."""


class NoSteamRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def request_steam(opener, request, limit, operation):
    try:
        with opener.open(request, timeout=10) as response:
            body = response.read(limit + 1)
    except HTTPError as error:
        status = error.code
        error.close()
        if operation == "library sync" and status in (401, 403):
            raise SteamSyncError(
                "Steam Web API rejected library access. Check the key and its permissions."
            ) from None
        raise SteamSyncError(f"Steam {operation} failed (HTTP {status}).") from None
    except (OSError, HTTPException):
        raise SteamSyncError(f"Could not reach Steam for {operation}.") from None
    if len(body) > limit:
        raise SteamSyncError(f"Steam {operation} response exceeded the size limit.")
    return body


def parse_openid_response(query):
    try:
        pairs = parse_qsl(query, strict_parsing=True, keep_blank_values=True,
                          errors="strict", max_num_fields=32)
    except ValueError:
        raise SteamSyncError("Malformed Steam sign-in response.") from None
    fields = dict(pairs)
    if len(fields) != len(pairs) or any(not key.startswith("openid.") for key in fields):
        raise SteamSyncError("Malformed Steam sign-in response.")
    return fields


def verify_openid(fields, return_to, opener):
    if fields.get("openid.mode") == "cancel":
        raise SteamSyncError("Steam sign-in was cancelled.")
    if (fields.get("openid.ns") != OPENID_NS or fields.get("openid.mode") != "id_res"
            or fields.get("openid.op_endpoint") != OPENID_PROVIDER
            or fields.get("openid.return_to") != return_to):
        raise SteamSyncError("Steam sign-in response does not match this request.")

    claimed_id = fields.get("openid.claimed_id", "")
    match = STEAM_ID.fullmatch(claimed_id)
    if match is None or fields.get("openid.identity") != claimed_id or int(match[1]) > 0xFFFFFFFFFFFFFFFF:
        raise SteamSyncError("Steam sign-in returned an invalid identity.")
    signed = set(fields.get("openid.signed", "").split(","))
    if (not REQUIRED_SIGNATURE_FIELDS <= signed or not fields.get("openid.sig")
            or any(not fields.get("openid." + name) for name in signed)):
        raise SteamSyncError("Steam sign-in response lacks signed identity fields.")

    nonce = fields["openid.response_nonce"]
    try:
        issued = datetime.strptime(nonce[:20], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError:
        raise SteamSyncError("Invalid Steam sign-in timestamp.") from None
    if (not 20 <= len(nonce) <= 255 or not all("!" <= ch <= "~" for ch in nonce[20:])
            or abs((datetime.now(timezone.utc) - issued).total_seconds()) > MAX_ASSERTION_AGE):
        raise SteamSyncError("Steam sign-in verification has expired or the timestamp is invalid.")

    # The Steam provider and identity namespace are pinned. Never use callback URLs for discovery.
    data = urlencode({**fields, "openid.mode": "check_authentication"}).encode("ascii")
    request = Request(OPENID_PROVIDER, data=data, headers={
        "Content-Type": "application/x-www-form-urlencoded",
    })
    response = request_steam(opener, request, MAX_CALLBACK_BYTES, "identity verification")
    verdict = {}
    for line in response.splitlines():
        key, separator, value = line.partition(b":")
        if not separator or key in verdict:
            raise SteamSyncError("Steam returned an invalid verification response.")
        verdict[key] = value
    if verdict.get(b"ns") != OPENID_NS.encode("ascii") or verdict.get(b"is_valid") != b"true":
        raise SteamSyncError("Steam could not verify this sign-in.")
    return match[1]


class SteamCallbackHandler(BaseHTTPRequestHandler):
    timeout = 2

    def log_message(self, format, *args):
        # A request URL contains the one-time path and an account assertion.
        pass

    def do_GET(self):
        if self.headers.get_all("Host") != [self.server.host] or len(self.path) > MAX_CALLBACK_BYTES:
            self.reply(400, "Invalid callback request.")
            return
        try:
            callback = urlsplit(self.path)
        except ValueError:
            self.reply(400, "Invalid callback request.")
            return
        if (callback.scheme or callback.netloc or callback.fragment
                or callback.path != self.server.callback_path):
            self.reply(404, "Callback not found.")
            return
        try:
            fields = parse_openid_response(callback.query)
            self.server.outcome = verify_openid(fields, self.server.return_to, self.server.opener)
        except SteamSyncError as error:
            self.server.outcome = error
            self.reply(400, "Steam sign-in did not complete. See the terminal for details.")
            return
        self.reply(200, "Steam sign-in verified. You can return to the terminal.")

    def reply(self, status, text):
        data = (text + "\n").encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(data)
        except OSError:
            # A browser can close the tab while the callback is being completed.
            pass


class SteamCallbackServer(HTTPServer):
    def __init__(self, opener):
        super().__init__(("127.0.0.1", 0), SteamCallbackHandler)
        self.opener = opener
        self.host = f"127.0.0.1:{self.server_port}"
        self.realm = f"http://{self.host}/"
        self.callback_path = "/steam/callback/" + secrets.token_urlsafe(32)
        self.return_to = self.realm.rstrip("/") + self.callback_path
        self.outcome = None


def sign_in(opener, open_browser, timeout):
    try:
        server = SteamCallbackServer(opener)
    except OSError:
        raise SteamSyncError("Could not start the local Steam sign-in callback.") from None
    with server:
        identifier = OPENID_NS + "/identifier_select"
        address = OPENID_PROVIDER + "?" + urlencode({
            "openid.ns": OPENID_NS,
            "openid.mode": "checkid_setup",
            "openid.realm": server.realm,
            "openid.return_to": server.return_to,
            "openid.claimed_id": identifier,
            "openid.identity": identifier,
        })
        launch = Future()

        def launch_browser():
            try:
                opened = open_browser(address)
            except BaseException as error:
                launch.set_exception(error)
            else:
                launch.set_result(opened)

        # A foreground launcher can wait for the browser to exit. Keep serving its callback.
        deadline = time.monotonic() + timeout
        Thread(target=launch_browser, daemon=True).start()
        while server.outcome is None:
            if launch.done():
                try:
                    if not launch.result():
                        raise SteamSyncError("Could not open the system browser for Steam sign-in.")
                except (OSError, webbrowser.Error):
                    raise SteamSyncError("Could not open the system browser for Steam sign-in.") from None
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise SteamSyncError("Timed out waiting for Steam sign-in.")
            server.timeout = min(remaining, 0.5)
            server.handle_request()
        if isinstance(server.outcome, SteamSyncError):
            raise server.outcome
        return server.outcome


def unique_json_object(pairs):
    value = dict(pairs)
    if len(value) != len(pairs):
        raise ValueError("duplicate JSON field")
    return value


def decode_json(data, source, *, encoding="utf-8-sig"):
    try:
        return json.loads(data.decode(encoding), object_pairs_hook=unique_json_object,
                          parse_int=json_integer, parse_constant=invalid_json_constant)
    except (ValueError, RecursionError):
        raise SteamSyncError(f"Invalid JSON in {source}.") from None


def json_integer(value):
    if len(value.lstrip("-")) > 20:
        raise ValueError("oversized JSON integer")
    return int(value)


def invalid_json_constant(value):
    raise ValueError("invalid JSON constant")


def fetch_library(opener, key, steam_id):
    params = urlencode({
        "key": key,
        "format": "json",
        "input_json": json.dumps({"steamid": int(steam_id), "include_appinfo": True}),
    })
    request = Request(OWNED_GAMES + "?" + params, headers={"Accept": "application/json"})
    body = request_steam(opener, request, 8 * 1024 * 1024, "library sync")
    document = decode_json(body, "the Steam library response")
    info = document.get("response") if type(document) is dict else None
    if info == {}:
        raise SteamSyncError("Steam did not expose this library to the Web API key. Check Game details visibility.")
    if type(info) is not dict or type(info.get("game_count")) is not int:
        raise SteamSyncError("Steam returned an invalid library response.")
    count = info["game_count"]
    games = info.get("games", [] if count == 0 else None)
    if count > MAX_CATALOG_ENTRIES:
        raise SteamSyncError("Steam library exceeds the HOME catalog entry limit.")
    if type(games) is not list or len(games) != count:
        raise SteamSyncError("Steam library response has an inconsistent game count.")

    apps = {}
    for game in games:
        app_id = game.get("appid") if type(game) is dict else None
        if type(app_id) is not int or not 0 < app_id <= 0xFFFFFFFF or app_id in apps:
            raise SteamSyncError("Steam library response contains an invalid or duplicate app ID.")
        name = game.get("name", "")
        if type(name) is not str:
            raise SteamSyncError("Steam library response contains an invalid game name.")
        name = name.strip()
        if any(0xD800 <= ord(ch) <= 0xDFFF for ch in name):
            raise SteamSyncError("Steam library response contains an invalid game name.")
        apps[app_id] = steam_entry(str(app_id), name or f"Steam App {app_id}")
    return [apps[app_id] for app_id in sorted(apps)]


def read_base_catalog(path):
    try:
        if not path.is_file():
            raise SteamSyncError("Base catalog must be a regular file.")
        with path.open("rb") as stream:
            data = stream.read(MAX_CATALOG_BYTES + 1)
    except OSError:
        raise SteamSyncError("Could not read the base catalog.") from None
    if len(data) > MAX_CATALOG_BYTES:
        raise SteamSyncError("Base catalog exceeds the HOME catalog size limit.")
    # Match HOME's support for output redirected by Windows PowerShell 5.1.
    # Only local snapshots opt in; Steam responses continue to require UTF-8.
    encoding = "utf-16" if data.startswith(b"\xff\xfe") else "utf-8-sig"
    catalog = decode_json(data, "the base catalog", encoding=encoding)
    if (type(catalog) is not dict or catalog.keys() != {"schema_version", "entries"}
            or type(catalog["schema_version"]) is not int or catalog["schema_version"] != 1
            or type(catalog["entries"]) is not list):
        raise SteamSyncError("Base catalog does not use schema version 1.")
    entries = catalog["entries"]
    if len(entries) > MAX_CATALOG_ENTRIES:
        raise SteamSyncError("Base catalog exceeds the HOME catalog entry limit.")

    other_providers = []
    identities = set()
    for entry in entries:
        if (type(entry) is not dict or entry.keys() != ENTRY_FIELDS
                or any(type(entry[key]) is not str or not entry[key] for key in ENTRY_TEXT_FIELDS)
                or any(entry[key] is not None and (type(entry[key]) is not str or not entry[key])
                       for key in ENTRY_OPTIONAL_TEXT_FIELDS)):
            raise SteamSyncError("Base catalog contains an invalid entry.")
        played = entry["last_played"]
        if played is not None and (type(played) is not int or not 0 <= played <= 0xFFFFFFFFFFFFFFFF):
            raise SteamSyncError("Base catalog contains an invalid play timestamp.")
        if any(isinstance(value, str) and any(0xD800 <= ord(ch) <= 0xDFFF for ch in value)
               for value in entry.values()):
            raise SteamSyncError("Base catalog contains invalid text.")
        if "\0" in entry["provider_id"]:
            raise SteamSyncError("Base catalog contains an invalid app identity.")
        identity = (entry["provider"], entry["provider_id"])
        if identity in identities:
            raise SteamSyncError("Base catalog contains duplicate app identities.")
        identities.add(identity)
        if entry["provider"] != "steam":
            other_providers.append(entry)
    return other_providers


def save_catalog(path, entries):
    try:
        write_catalog_file(path, entries)
    except (CatalogLimitError, CatalogOutputError) as exc:
        raise SteamSyncError(str(exc)) from None


def sync_library(output, key, *, base_catalog=None, timeout=180, open_browser=None, opener=None):
    if re.fullmatch(r"[0-9a-fA-F]{32}", key or "") is None:
        raise SteamSyncError(f"Set {KEY_ENV} to a Steam Web API key before running the command.")
    if opener is None:
        opener = build_opener(NoSteamRedirect)
    previous = read_base_catalog(base_catalog) if base_catalog is not None else []
    try:
        output_parent_is_directory = output.parent.is_dir()
    except OSError:
        output_parent_is_directory = False
    if not output_parent_is_directory:
        raise SteamSyncError("Catalog output requires an accessible directory.")
    steam_id = sign_in(opener, open_browser or webbrowser.open, timeout)
    steam = fetch_library(opener, key, steam_id)
    save_catalog(output, steam + previous)
    return len(steam)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path, help="Catalog JSON path to replace on success.")
    parser.add_argument("--base-catalog", type=Path, help="Keep non-Steam entries from this schema-v1 snapshot.")
    parser.add_argument("--timeout", type=int, default=180, metavar="SECONDS",
                        help="Time to complete sign-in in the browser (default: 180).")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout <= 900:
        parser.error("--timeout must be between 1 and 900 seconds")
    # Remove the key from this process's environment before opening a child browser.
    key = os.environ.pop(KEY_ENV, None)
    try:
        count = sync_library(args.output, key, base_catalog=args.base_catalog, timeout=args.timeout)
    except SteamSyncError as error:
        print(error, file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Steam library sync interrupted.", file=sys.stderr)
        return 1
    print(f"Updated the catalog with {count} Steam entries.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
