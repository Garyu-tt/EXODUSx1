"""Check an active Steam license for one publisher-configured AppID."""

import argparse
from enum import Enum
from http.client import HTTPException
import json
import os
import re
import sys
from urllib.error import HTTPError
from urllib.parse import urlencode
from urllib.request import ProxyHandler, Request, build_opener
import webbrowser

from .steam_sync import (
    KEY_ENV as LIBRARY_KEY_ENV,
    NoSteamRedirect,
    SteamSyncError,
    decode_json,
    sign_in,
)


PUBLISHER_APP_ID_ENV = "EXODUS_STEAM_PUBLISHER_APP_ID"
PUBLISHER_KEY_ENV = "STEAMWORKS_PUBLISHER_KEY"
CHECK_APP_OWNERSHIP = "https://partner.steam-api.com/ISteamUser/CheckAppOwnership/v4/"
MAX_OWNERSHIP_BYTES = 8192
MAX_APP_ID = 0xFFFFFFFF
MAX_STEAM_ID = 0xFFFFFFFFFFFFFFFF
DECIMAL = re.compile(r"[1-9][0-9]{0,19}", re.ASCII)
PUBLISHER_KEY = re.compile(r"[0-9a-fA-F]{32}", re.ASCII)


class LicenseStatus(str, Enum):
    ACTIVE_LICENSE = "active_license"
    NO_ACTIVE_LICENSE = "no_active_license"
    UNSUPPORTED_APP = "unsupported_app"
    PUBLISHER_NOT_CONFIGURED = "publisher_not_configured"
    SIGN_IN_UNAVAILABLE = "sign_in_unavailable"
    OWNERSHIP_UNAVAILABLE = "ownership_unavailable"


def parse_identifier(value, maximum):
    if type(value) is str:
        if DECIMAL.fullmatch(value) is None:
            return None
        value = int(value)
    if type(value) is int and 1 <= value <= maximum:
        return value
    return None


def _status_from_reply(document, app_id, steam_id):
    unavailable = LicenseStatus.OWNERSHIP_UNAVAILABLE
    if type(document) is not dict or "response" in document or "error" in document:
        return unavailable

    ownership = document
    if "appownership" in document:
        ownership = document["appownership"]
        if (document.keys() - {"appownership", "appid", "steamid"}
                or type(ownership) is not dict
                or {"appownership", "response"} & ownership.keys()):
            return unavailable

    for fields in (document, ownership):
        if "appid" in fields and parse_identifier(fields["appid"], MAX_APP_ID) != app_id:
            return unavailable
        if "steamid" in fields and parse_identifier(fields["steamid"], MAX_STEAM_ID) != steam_id:
            return unavailable

    owns_app = ownership.get("ownsapp")
    canceled = ownership.get("usercanceled", False)
    if (type(owns_app) is not bool or type(canceled) is not bool
            or (owns_app and canceled) or "error" in ownership):
        return unavailable
    return LicenseStatus.ACTIVE_LICENSE if owns_app else LicenseStatus.NO_ACTIVE_LICENSE


def _read_ownership_reply(opener, request):
    try:
        with opener.open(request, timeout=10) as response:
            if response.getcode() != 200:
                return None
            declared = response.headers.get_all("Content-Length", [])
            expected_bytes = None
            if declared:
                if len(declared) != 1 or "Transfer-Encoding" in response.headers:
                    return None
                value = declared[0].strip(" \t")
                if re.fullmatch(r"[0-9]{1,10}", value, re.ASCII) is None:
                    return None
                expected_bytes = int(value)
                if expected_bytes > MAX_OWNERSHIP_BYTES:
                    return None
            body = response.read(MAX_OWNERSHIP_BYTES + 1)
    except HTTPError as error:
        error.close()
        return None
    except (OSError, HTTPException, ValueError):
        return None
    if len(body) > MAX_OWNERSHIP_BYTES or (expected_bytes is not None and len(body) != expected_bytes):
        return None
    return body


def check_entitlement(app_id, *, publisher_app_id, publisher_key, timeout=180, opener=None, open_browser=None):
    """Return a live license status for an identity confirmed through Steam OpenID."""
    app_id = parse_identifier(app_id, MAX_APP_ID)
    publisher_app_id = parse_identifier(publisher_app_id, MAX_APP_ID)
    if publisher_app_id is None:
        return LicenseStatus.PUBLISHER_NOT_CONFIGURED
    if app_id != publisher_app_id:
        return LicenseStatus.UNSUPPORTED_APP
    if type(publisher_key) is not str or PUBLISHER_KEY.fullmatch(publisher_key) is None:
        return LicenseStatus.PUBLISHER_NOT_CONFIGURED
    if type(timeout) is not int or not 1 <= timeout <= 900:
        return LicenseStatus.SIGN_IN_UNAVAILABLE

    if opener is None:
        opener = build_opener(ProxyHandler({}), NoSteamRedirect)
    try:
        steam_id = parse_identifier(sign_in(opener, open_browser or webbrowser.open, timeout), MAX_STEAM_ID)
    except (SteamSyncError, ValueError, KeyboardInterrupt):
        return LicenseStatus.SIGN_IN_UNAVAILABLE
    if steam_id is None:
        return LicenseStatus.SIGN_IN_UNAVAILABLE

    params = urlencode({"appid": app_id, "steamid": steam_id, "format": "json"})
    request = Request(CHECK_APP_OWNERSHIP + "?" + params, headers={"Accept": "application/json"})
    request.add_unredirected_header("X-WebAPI-Key", publisher_key)
    try:
        body = _read_ownership_reply(opener, request)
        if body is None:
            return LicenseStatus.OWNERSHIP_UNAVAILABLE
        document = decode_json(body, "the Steam ownership response")
    except (SteamSyncError, KeyboardInterrupt):
        return LicenseStatus.OWNERSHIP_UNAVAILABLE
    return _status_from_reply(document, app_id, steam_id)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app-id", required=True, help="canonical decimal Steam AppID")
    parser.add_argument("--timeout", type=int, default=180, metavar="SECONDS",
                        help="time to complete Steam sign-in in the browser")
    args = parser.parse_args(argv)
    app_id = parse_identifier(args.app_id, MAX_APP_ID)
    if app_id is None:
        parser.error("--app-id must be a canonical positive uint32")
    if not 1 <= args.timeout <= 900:
        parser.error("--timeout must be between 1 and 900 seconds")

    # Keep the API keys out of the environment inherited by the browser.
    publisher_key = os.environ.pop(PUBLISHER_KEY_ENV, None)
    os.environ.pop(LIBRARY_KEY_ENV, None)
    try:
        status = check_entitlement(app_id, publisher_app_id=os.environ.get(PUBLISHER_APP_ID_ENV),
                                   publisher_key=publisher_key, timeout=args.timeout)
    except KeyboardInterrupt:
        status = LicenseStatus.OWNERSHIP_UNAVAILABLE
    print(json.dumps({"appid": app_id, "status": status.value}))
    return 0 if status in (LicenseStatus.ACTIVE_LICENSE, LicenseStatus.NO_ACTIVE_LICENSE) else 1


if __name__ == "__main__":
    sys.exit(main())
