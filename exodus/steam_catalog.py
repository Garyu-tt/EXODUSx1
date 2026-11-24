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
    """Parse nested KeyValues with case-insensitive first-match lookup."""
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
            # Parse repeated blocks even after the first value has been retained.
            stack[-1].setdefault(key, child)
            stack.append(child)
            key = None
        elif key is None:
            key = token_text(token).lower()
        else:
            stack[-1].setdefault(key, token_text(token))
            key = None
    if len(stack) != 1 or key is not None:
        raise ValueError("incomplete key/value object")
    return root


def read_manifest(manifest, *, kind="manifest"):
    if not manifest.is_file():
        raise ValueError(f"expected a regular {kind} file")
    with manifest.open("rb") as stream:
        data = stream.read(MAX_MANIFEST_BYTES + 1)
    if len(data) > MAX_MANIFEST_BYTES:
        raise ValueError(f"{kind} exceeds 16 MiB")
    return parse_keyvalues(data.decode("utf-8-sig"))


def discover_libraries(installations):
    """Read library roots from explicitly selected Steam installations."""
    libraries = []
    errors = False
    seen = set()
    for installation in installations:
        # Keep the installation's default library even when discovery fails.
        libraries.append(installation / "steamapps")
        configuration = installation / "steamapps" / "libraryfolders.vdf"
        try:
            canonical = installation.resolve()
            if canonical in seen:
                continue
            seen.add(canonical)
            try:
                configuration.stat()
            except FileNotFoundError:
                configuration = installation / "config" / "libraryfolders.vdf"
            folders = read_manifest(configuration, kind="library configuration").get("libraryfolders")
            if not isinstance(folders, dict):
                raise ValueError("missing LibraryFolders object")
        except (ValueError, OSError, RuntimeError) as exc:
            print(f"{configuration}: {exc}", file=sys.stderr)
            errors = True
            continue

        # Numeric order is deterministic even if the VDF entries are reordered.
        # Sort decimal strings without converting arbitrarily long keys to int.
        indexes = [key for key in folders if re.fullmatch(r"[0-9]+", key)]
        indexes.sort(key=lambda key: (len(key.lstrip("0")), key.lstrip("0"), key))
        for index in indexes:
            value = folders[index]
            path = value.get("path") if isinstance(value, dict) else value
            if (not isinstance(path, str) or not path or "\0" in path
                    or not Path(path).is_absolute()):
                print(f"{configuration}: library {index} requires an absolute path for this host",
                      file=sys.stderr)
                errors = True
                continue
            libraries.append(Path(path))
    return libraries, errors


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
    return steam_entry(app_id, name.strip(), installation, optional_uint32(state, "lastplayed") or None)


def steam_entry(app_id, name, installation="unknown", last_played=None):
    return {
        "provider": "steam",
        "provider_id": app_id,
        "name": name,
        "artwork": None,
        "installation_state": installation,
        "last_played": last_played,
        "compatibility_state": "unknown",
        "launch_mode": None,
    }


def scan_libraries(libraries, *, steam_installs=()):
    entries = {}
    discovered, errors = discover_libraries(steam_installs)
    scanned = set()
    for library in list(libraries) + discovered:
        nested = library / "steamapps"
        steamapps = library if library.name.lower() == "steamapps" else nested
        try:
            if steamapps.resolve() in scanned:
                continue
            manifests = sorted(steamapps.iterdir())
            # A library root can share the name of its manifest directory.
            if (steamapps == library
                    and not any(MANIFEST.fullmatch(path.name) for path in manifests)
                    and nested.is_dir()):
                steamapps = nested
                manifests = sorted(steamapps.iterdir())
            canonical = steamapps.resolve()
            if canonical in scanned:
                continue
            scanned.add(canonical)
        except (OSError, RuntimeError) as exc:
            print(f"{steamapps}: {getattr(exc, 'strerror', None) or exc}", file=sys.stderr)
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

    return [entries[k] for k in sorted(entries, key=int)], errors


def write_catalog(entries, stream=None):
    stream = sys.stdout if stream is None else stream
    catalog = {"schema_version": 1, "entries": entries}
    json.dump(catalog, stream, ensure_ascii=True, indent=2)
    stream.write("\n")


def main():
    parser = argparse.ArgumentParser(
        description="Read local Steam app manifests and write an offline metadata catalog to stdout."
    )
    parser.add_argument("libraries", metavar="LIBRARY", type=Path, nargs="*",
                        help="Steam library root containing steamapps, or the steamapps directory itself")
    parser.add_argument("--steam-install", metavar="ROOT", type=Path, action="append", default=[],
                        help="Steam installation root whose libraryfolders.vdf lists libraries to scan. May be repeated.")
    args = parser.parse_intermixed_args()
    if not args.libraries and not args.steam_install:
        parser.error("supply at least one LIBRARY or --steam-install")
    entries, errors = scan_libraries(args.libraries, steam_installs=args.steam_install)
    write_catalog(entries)
    return int(errors)


if __name__ == "__main__":
    sys.exit(main())
