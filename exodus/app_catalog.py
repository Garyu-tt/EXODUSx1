"""Combine mounted Switch homebrew and local Steam manifests in a catalog."""

import argparse
import os
from pathlib import Path
import struct
import sys
import zlib

from .steam_catalog import scan_libraries, write_catalog


# Layouts from libnx's switch/nro.h and switch/nacp.h.
NRO_HEADER_SIZE = 0x80
NRO_HEADER_OFFSET = 0x10
NRO_MAGIC_AND_SIZE = struct.Struct("<4s4xI")
# The icon and RomFS sections are irrelevant to the catalog.
ASSET_HEADER = struct.Struct("<4sI16xQQ16x")
NACP_SIZE = 0x4000
NACP_TITLE_FORMAT_OFFSET = 0x3215
NACP_LANGUAGE_SIZE = 0x300
NACP_NAME_SIZE = 0x200
NACP_INLINE_TITLES_SIZE = 16 * NACP_LANGUAGE_SIZE
NACP_EXPANDED_TITLES_SIZE = 32 * NACP_LANGUAGE_SIZE


def title_table(nacp):
    title_format = nacp[NACP_TITLE_FORMAT_OFFSET]
    if title_format == 0:
        return nacp[:NACP_INLINE_TITLES_SIZE]
    if title_format != 1:
        raise ValueError("unknown NACP title format")

    packed_size, = struct.unpack_from("<H", nacp)
    if not 0 < packed_size <= NACP_INLINE_TITLES_SIZE - 2:
        raise ValueError("invalid compressed NACP title length")
    inflater = zlib.decompressobj(-zlib.MAX_WBITS)
    titles = inflater.decompress(nacp[2:2 + packed_size], NACP_EXPANDED_TITLES_SIZE + 1)
    if len(titles) != NACP_EXPANDED_TITLES_SIZE or not inflater.eof or inflater.unused_data:
        raise ValueError("invalid compressed NACP title table")
    return titles


def homebrew_name(path):
    if not path.is_file():
        raise ValueError("expected a regular NRO file")
    with path.open("rb") as stream:
        file_size = os.fstat(stream.fileno()).st_size
        header = stream.read(NRO_HEADER_SIZE)
        if len(header) != NRO_HEADER_SIZE:
            raise ValueError("truncated NRO header")
        magic, nro_size = NRO_MAGIC_AND_SIZE.unpack_from(header, NRO_HEADER_OFFSET)
        if magic != b"NRO0" or not NRO_HEADER_SIZE <= nro_size <= file_size:
            raise ValueError("invalid NRO header or size")
        if nro_size == file_size:
            return path.stem

        stream.seek(nro_size)
        assets = stream.read(ASSET_HEADER.size)
        if len(assets) != ASSET_HEADER.size:
            raise ValueError("truncated NRO asset header")
        magic, version, offset, length = ASSET_HEADER.unpack(assets)
        if magic != b"ASET" or version != 0:
            raise ValueError("invalid NRO asset header")
        if not length:
            return path.stem
        if (length != NACP_SIZE or offset < ASSET_HEADER.size
                or offset + length > file_size - nro_size):
            raise ValueError("invalid NACP section bounds")

        stream.seek(nro_size + offset)
        nacp = stream.read(NACP_SIZE)
        if len(nacp) != NACP_SIZE:
            raise ValueError("truncated NACP section")

    titles = title_table(nacp)
    for start in range(0, len(titles), NACP_LANGUAGE_SIZE):
        raw_name = titles[start:start + NACP_NAME_SIZE].split(b"\0", 1)[0]
        name = raw_name.decode("utf-8").strip()
        if name:
            return name
    return path.stem


def scan_homebrew(sd_root):
    switch_dir = sd_root / "switch"
    entries = []
    errors = False

    def report_error(error):
        nonlocal errors
        print(error, file=sys.stderr)
        errors = True

    for directory, subdirs, files in os.walk(switch_dir, onerror=report_error):
        subdirs.sort()
        for filename in sorted(files):
            path = Path(directory) / filename
            if path.suffix.lower() != ".nro":
                continue
            try:
                if path.is_symlink():
                    continue
                name = homebrew_name(path)
            except (ValueError, OSError, zlib.error) as exc:
                report_error(f"{path}: {exc}")
                continue
            entries.append({
                "provider": "switch_homebrew",
                "provider_id": path.relative_to(switch_dir).as_posix(),
                "name": name,
                "artwork": None,
                "installation_state": "reported_installed",
                "last_played": None,
                "compatibility_state": "unknown",
                "launch_mode": None,
            })
    entries.sort(key=lambda entry: entry["provider_id"])
    return entries, errors


def main():
    parser = argparse.ArgumentParser(
        description="Write an offline catalog from Steam libraries and a mounted Switch SD card."
    )
    parser.add_argument("--steam-library", metavar="LIBRARY", type=Path, action="append", default=[],
                        help="Steam library root or steamapps directory. May be repeated.")
    parser.add_argument("--switch-sd", metavar="ROOT", type=Path,
                        help="Mounted Switch SD card root containing the switch directory.")
    args = parser.parse_args()
    if not args.steam_library and args.switch_sd is None:
        parser.error("supply at least one --steam-library or --switch-sd")

    steam, steam_errors = scan_libraries(args.steam_library)
    homebrew, homebrew_errors = scan_homebrew(args.switch_sd) if args.switch_sd else ([], False)
    write_catalog(steam + homebrew)
    return int(steam_errors or homebrew_errors)


if __name__ == "__main__":
    sys.exit(main())
