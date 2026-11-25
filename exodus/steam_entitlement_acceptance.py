"""Run the three publisher acceptance checks for one Steam AppID."""

import argparse
import json
import sys

from .steam_entitlement import (
    MAX_APP_ID,
    MAX_STEAM_ID,
    LicenseStatus,
    check_entitlement,
    parse_identifier,
    publisher_configuration_status,
    take_publisher_configuration,
)


def _report(app_id, status, cases):
    print(json.dumps({"appid": app_id, "status": status, "cases": cases}))
    return 0 if status == "passed" else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--app-id", required=True, help="canonical decimal Steam AppID")
    parser.add_argument("--licensed-steam-id", required=True, help="SteamID64 of the licensed test account")
    parser.add_argument("--unlicensed-steam-id", required=True, help="SteamID64 of the unlicensed test account")
    parser.add_argument("--timeout", default=180, metavar="SECONDS", help="time for each Steam sign-in")
    args, unknown = parser.parse_known_args(argv)
    if unknown:
        parser.error("unrecognized arguments")

    app_id = parse_identifier(args.app_id, MAX_APP_ID)
    licensed = parse_identifier(args.licensed_steam_id, MAX_STEAM_ID)
    unlicensed = parse_identifier(args.unlicensed_steam_id, MAX_STEAM_ID)
    timeout = parse_identifier(args.timeout, 900)
    if app_id is None:
        parser.error("--app-id must be a canonical positive uint32")
    if licensed is None or unlicensed is None or licensed == unlicensed:
        parser.error("provide two distinct canonical positive SteamID64 values")
    if timeout is None:
        parser.error("--timeout must be between 1 and 900 seconds")

    publisher_app_id, publisher_key = take_publisher_configuration()
    configuration_error = publisher_configuration_status(app_id, publisher_app_id, publisher_key)
    if configuration_error is not None:
        return _report(app_id, configuration_error.value, [])

    scenarios = (
        ("licensed", licensed, LicenseStatus.ACTIVE_LICENSE, "Choose the licensed test account in Steam."),
        ("unlicensed", unlicensed, LicenseStatus.NO_ACTIVE_LICENSE, "Choose the unlicensed test account in Steam."),
        ("wrong_account", licensed, LicenseStatus.ACCOUNT_MISMATCH,
         "For the mismatch check, choose the unlicensed test account in Steam."),
    )
    cases = []
    for label, expected_id, expected_status, instruction in scenarios:
        print(f"{instruction} Press Enter when ready, or type q to stop.", file=sys.stderr, flush=True)
        try:
            if sys.stdin.readline(3) not in ("\n", "\r\n"):
                return _report(app_id, "aborted", cases)
            observed = check_entitlement(app_id, publisher_app_id=publisher_app_id, publisher_key=publisher_key,
                                         expected_steam_id=expected_id, timeout=timeout)
        except (EOFError, OSError, KeyboardInterrupt):
            return _report(app_id, "aborted", cases)
        cases.append({"case": label, "expected": expected_status.value, "observed": observed.value})
        if observed != expected_status:
            return _report(app_id, "failed", cases)

    return _report(app_id, "passed", cases)


if __name__ == "__main__":
    sys.exit(main())
