"""Compose local EXODUS catalog snapshots using provider and provider ID."""

import argparse
import json
from pathlib import Path
import sys


MAX_CATALOG_BYTES = 16 * 1024 * 1024
TEXT_FIELDS = ("provider", "provider_id", "name", "installation_state", "compatibility_state")
OPTIONAL_TEXT_FIELDS = ("artwork", "launch_mode")
ENTRY_FIELDS = set(TEXT_FIELDS + OPTIONAL_TEXT_FIELDS + ("last_played",))


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("duplicate JSON object key")
        value[key] = item
    return value


def catalog_integer(text):
    if len(text) > 20:
        raise ValueError("catalog integer exceeds 20 digits")
    return int(text)


def read_catalog(path):
    if not path.is_file():
        raise ValueError("expected a regular catalog file")
    with path.open("rb") as stream:
        data = stream.read(MAX_CATALOG_BYTES + 1)
    if len(data) > MAX_CATALOG_BYTES:
        raise ValueError("catalog exceeds 16 MiB")
    catalog = json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique_object,
                         parse_int=catalog_integer)
    if (not isinstance(catalog, dict) or catalog.keys() != {"schema_version", "entries"}
            or type(catalog["schema_version"]) is not int or catalog["schema_version"] != 1
            or not isinstance(catalog["entries"], list)):
        raise ValueError("expected an EXODUS schema-v1 catalog")

    for number, entry in enumerate(catalog["entries"], 1):
        if not isinstance(entry, dict) or entry.keys() != ENTRY_FIELDS:
            raise ValueError(f"invalid fields in catalog entry {number}")
        for field in TEXT_FIELDS + OPTIONAL_TEXT_FIELDS:
            value = entry[field]
            if value is None and field in OPTIONAL_TEXT_FIELDS:
                continue
            if not isinstance(value, str) or not value:
                raise ValueError(f"invalid {field} in catalog entry {number}")
            if field in ("provider", "provider_id") and value != value.strip():
                raise ValueError(f"invalid {field} in catalog entry {number}")
            value.encode("utf-8")
        last_played = entry["last_played"]
        if last_played is not None and (type(last_played) is not int
                                        or not 0 <= last_played <= 0xFFFFFFFFFFFFFFFF):
            raise ValueError(f"invalid last_played in catalog entry {number}")
    return catalog["entries"]


def main():
    parser = argparse.ArgumentParser(description="Merge local EXODUS schema-v1 catalogs.")
    parser.add_argument("catalogs", metavar="CATALOG", type=Path, nargs="+")
    entries = {}
    for path in parser.parse_args().catalogs:
        try:
            for entry in read_catalog(path):
                identity = (entry["provider"], entry["provider_id"])
                previous = entries.setdefault(identity, entry)
                if previous != entry:
                    raise ValueError(f"conflicting entries for {identity[0]!r} ID {identity[1]!r}")
        except (OSError, ValueError, RecursionError) as exc:
            print(f"{path}: {exc}", file=sys.stderr)
            return 1

    json.dump({"schema_version": 1, "entries": list(entries.values())}, sys.stdout,
              ensure_ascii=True, indent=2)
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
