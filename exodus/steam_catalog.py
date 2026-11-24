"""Import local Steam app manifests into an offline EXODUS catalog."""

import argparse
import json
from pathlib import Path
import re
import sys


# Steam app manifests use a subset of Valve's text KeyValues format.
TOKEN = re.compile(r'\s+|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+|.', re.DOTALL)
MANIFEST = re.compile(r"appmanifest_([0-9]+)\.acf", re.IGNORECASE | re.ASCII)
MAX_MANIFEST_BYTES = 16 * 1024 * 1024
FULLY_INSTALLED = 4
STATE_PRIORITY = {"unknown": 0, "not_fully_installed": 1, "reported_installed": 2}


def token_text(token):
    if token.startswith('"'):
        if len(token) == 1:
            raise ValueError("unterminated quoted string")
        return re.sub(r'\\(["\\])', r'\1', token[1:-1])
    return token


def parse_keyvalues(text):
    """Parse key/value pairs, keeping nested objects separate from AppState."""
    root = {}
    stack = [root]
    key = None
    for match in TOKEN.finditer(text):
        token = match[0]
        if token.isspace() or token.startswith("//"):
            continue
        if token == "}":
            if key is not None or len(stack) == 1:
                raise ValueError("unexpected closing brace")
            stack.pop()
        elif token == "{":
            if key is None:
                raise ValueError("object is missing a key")
            child = {}
            stack[-1][key] = child
            stack.append(child)
            key = None
        elif key is None:
            key = token_text(token).lower()
        else:
            stack[-1][key] = token_text(token)
            key = None
    if len(stack) != 1 or key is not None:
        raise ValueError("incomplete key/value object")
    return root


def read_manifest(manifest):
    if not manifest.is_file():
        raise ValueError("expected a regular manifest file")
    with manifest.open("rb") as stream:
        data = stream.read(MAX_MANIFEST_BYTES + 1)
    if len(data) > MAX_MANIFEST_BYTES:
        raise ValueError("manifest exceeds 16 MiB")
    return parse_keyvalues(data.decode("utf-8-sig"))


def optional_uint32(state, key):
    if key not in state:
        return None
    value = state[key]
    if isinstance(value, str) and re.fullmatch(r"[0-9]{1,10}", value):
        number = int(value)
        if number <= 0xFFFFFFFF:
            return number
    raise ValueError(f"invalid {key}")


def catalog_entry(manifest, expected_id):
    state = read_manifest(manifest).get("appstate")
    if not isinstance(state, dict):
        raise ValueError("missing AppState object")
    app_id = state.get("appid")
    valid_id = re.fullmatch(r"[1-9][0-9]{0,9}", expected_id)
    if app_id != expected_id or not valid_id or int(expected_id) > 0xFFFFFFFF:
        raise ValueError("appid must be a positive Steam App ID matching the filename")
    name = state.get("name")
    if not isinstance(name, str) or not name.strip():
        raise ValueError("missing application name")

    installation = "unknown"
    flags = optional_uint32(state, "stateflags")
    if flags is not None:
        installation = "reported_installed" if flags & FULLY_INSTALLED else "not_fully_installed"
    return {
        "provider": "steam",
        "provider_id": app_id,
        "name": name.strip(),
        "artwork": None,
        "installation_state": installation,
        "last_played": optional_uint32(state, "lastplayed") or None,
        "compatibility_state": "unknown",
        "launch_mode": None,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Read local Steam app manifests and write an offline metadata catalog to stdout."
    )
    parser.add_argument("libraries", metavar="LIBRARY", type=Path, nargs="+",
                        help="Steam library root containing steamapps, or the steamapps directory itself")
    entries = {}
    errors = False
    for library in parser.parse_args().libraries:
        steamapps = library if library.name.lower() == "steamapps" else library / "steamapps"
        try:
            manifests = sorted(steamapps.iterdir())
        except OSError as exc:
            print(f"{steamapps}: {exc.strerror}", file=sys.stderr)
            errors = True
            continue
        for manifest in manifests:
            match = MANIFEST.fullmatch(manifest.name)
            if not match:
                continue
            try:
                entry = catalog_entry(manifest, match[1])
            except (ValueError, OSError) as exc:
                print(f"{manifest}: {exc}", file=sys.stderr)
                errors = True
                continue
            app_id = entry["provider_id"]
            previous = entries.get(app_id)
            rank = STATE_PRIORITY[entry["installation_state"]]
            if previous is None or rank > STATE_PRIORITY[previous["installation_state"]]:
                entries[app_id] = entry

    catalog = {"schema_version": 1, "entries": [entries[k] for k in sorted(entries, key=int)]}
    json.dump(catalog, sys.stdout, ensure_ascii=True, indent=2)
    print()
    return int(errors)


if __name__ == "__main__":
    sys.exit(main())
