"""Read CorkDeck v2/v3 launcher-library-v2.ini as EXODUS catalog v1.

Read-only metadata importer, not a launcher. Format reference:
Garyu-tt/CorkDeck@adc0b52284e9e57d18808427a770a2a162c76bf0
horizon-wine/source/launcher_catalog.c and launcher_list.h.
"""
import argparse
from pathlib import Path
import re
import sys
from urllib.parse import unquote_to_bytes

from .steam_catalog import write_catalog

MAX_BYTES = 2 * 1024 * 1024
MAX_ENTRIES = 256
SECTION = re.compile(r"\[game ([1-9][0-9]{0,9})\]")
BAD_ESCAPE = re.compile(r"%(?![0-9a-fA-F]{2})")


def program_path_key(path):
    """Match CorkDeck's ASCII-insensitive comparison of UTF-8 path bytes."""
    return path.encode("utf-8").lower()


def decode_value(value, limit):
    if BAD_ESCAPE.search(value):
        raise ValueError("invalid percent escape")
    data = unquote_to_bytes(value)
    if len(data) >= limit or any(c < 32 or c == 127 for c in data):
        raise ValueError("invalid or oversized catalog field")
    return data.decode("utf-8")


def parse_catalog(text):
    if len(text.encode("utf-8")) > MAX_BYTES:
        raise ValueError("catalog too large")
    header = {}
    records = []
    ids = set()
    current = None
    for raw in text.split("\n"):
        if len(raw.encode("utf-8")) > 1198 or "\0" in raw:
            raise ValueError("invalid catalog line")
        line = raw.strip(" \t\r")
        if not line or line.startswith(("#", ";")):
            continue
        if line.startswith("["):
            match = SECTION.fullmatch(line)
            if not match or len(records) >= MAX_ENTRIES:
                raise ValueError("invalid or excessive game sections")
            number = int(match[1])
            if number >= 0xffffffff or number in ids:
                raise ValueError("invalid or duplicate game ID")
            ids.add(number)
            current = {}
            records.append(current)
            continue
        key, sep, value = line.partition("=")
        key, value = key.strip(" \t"), value.strip(" \t")
        target = header if current is None else current
        if not sep or not key or key in target:
            raise ValueError("invalid or duplicate key")
        target[key] = value
    if header.get("version") not in ("2", "3"):
        raise ValueError("unsupported CorkDeck catalog version")
    entries = []
    paths = set()
    for record in records:
        path = decode_value(record.get("path", ""), 512)
        title = decode_value(record.get("title", ""), 128)
        identity = program_path_key(path)
        if not path or identity in paths:
            raise ValueError("missing or duplicate program path")
        paths.add(identity)
        covers = {}
        for key, value in record.items():
            if key in ("portrait-art", "landscape-art", "square-art", "hero-art"):
                # The donor stores both aliases in the same portrait field.
                # Validate each supplied value before a later alias replaces it.
                slot = "portrait-art" if key == "landscape-art" else key
                covers[slot] = decode_value(value, 512)
        artwork = (covers.get("portrait-art") or covers.get("square-art")
                   or covers.get("hero-art") or None)
        entries.append({
            "provider": "corkdeck",
            "provider_id": path,
            "name": title.strip() or path.rsplit("/", 1)[-1] or path,
            "artwork": artwork,
            "installation_state": "unknown",
            "last_played": None,
            "compatibility_state": "unknown",
            "launch_mode": None,
        })
    return entries


def read_catalog(path):
    if not path.is_file():
        raise ValueError("expected a regular catalog file")
    with path.open("rb") as stream:
        data = stream.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise ValueError("catalog too large")
    return parse_catalog(data.decode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    args = parser.parse_args()
    try:
        entries = read_catalog(args.catalog)
    except (ValueError, OSError) as exc:
        print(f"CorkDeck catalog: {exc}", file=sys.stderr)
        return 1
    write_catalog(entries)
    return 0


if __name__ == "__main__":
    sys.exit(main())
