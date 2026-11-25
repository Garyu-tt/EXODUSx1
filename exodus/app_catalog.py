"""Combine Steam, mounted Switch homebrew and CorkDeck libraries in a catalog."""

import argparse
import os
from pathlib import Path
import stat
import struct
import sys
import zlib

from .corkdeck_catalog import program_path_key, read_catalog as read_corkdeck_catalog
from .steam_catalog import (
    CatalogLimitError,
    CatalogOutputError,
    scan_libraries,
    write_catalog,
    write_catalog_file,
)


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
    decode_error = None
    for start in range(0, len(titles), NACP_LANGUAGE_SIZE):
        raw_name = titles[start:start + NACP_NAME_SIZE].split(b"\0", 1)[0]
        try:
            name = raw_name.decode("utf-8").strip()
        except UnicodeDecodeError as exc:
            if decode_error is None:
                decode_error = exc
            continue
        if name:
            return name
    if decode_error is not None:
        raise decode_error
    return path.stem


def _is_windows_junction(path):
    try:
        return path.lstat().st_reparse_tag == stat.IO_REPARSE_TAG_MOUNT_POINT
    except OSError:
        return False


def scan_homebrew(sd_root):
    switch_dir = sd_root / "switch"
    entries = []
    errors = False

    def report_error(error):
        nonlocal errors
        print(error, file=sys.stderr)
        errors = True

    for directory, subdirs, files in os.walk(switch_dir, onerror=report_error):
        folder = Path(directory)
        if os.name == "nt":
            # The Windows walk can enter junctions with followlinks disabled.
            subdirs[:] = [name for name in subdirs if not _is_windows_junction(folder / name)]
        subdirs.sort()
        for filename in sorted(files):
            path = folder / filename
            if path.suffix.lower() != ".nro":
                continue
            try:
                if path.is_symlink():
                    continue
                provider_id = path.relative_to(switch_dir).as_posix()
                # Filesystem surrogate escapes cannot be read as text by HOME.
                provider_id.encode("utf-8")
                name = homebrew_name(path)
            except UnicodeEncodeError:
                report_error(f"{str(path)!r}: NRO path is not valid UTF-8")
                continue
            except (ValueError, OSError, zlib.error) as exc:
                report_error(f"{path}: {exc}")
                continue
            entries.append({
                "provider": "switch_homebrew",
                "provider_id": provider_id,
                "name": name,
                "artwork": None,
                "installation_state": "reported_installed",
                "last_played": None,
                "compatibility_state": "unknown",
                "launch_mode": None,
            })
    entries.sort(key=lambda entry: entry["provider_id"])
    return entries, errors


def scan_corkdeck_catalogs(catalogs):
    """Keep the first metadata record for each CorkDeck program identity."""
    entries = {}
    errors = False
    for path in catalogs:
        try:
            source = read_corkdeck_catalog(path)
        except (ValueError, OSError) as exc:
            print(f"{path}: {exc}", file=sys.stderr)
            errors = True
            continue
        for entry in source:
            entries.setdefault(program_path_key(entry["provider_id"]), entry)
    return list(entries.values()), errors


def main():
    parser = argparse.ArgumentParser(
        description="Write an offline catalog from Steam, Switch homebrew and CorkDeck libraries."
    )
    parser.add_argument("--steam-library", metavar="LIBRARY", type=Path, action="append", default=[],
                        help="Steam library root or steamapps directory. May be repeated.")
    parser.add_argument("--steam-install", metavar="ROOT", type=Path, action="append", default=[],
                        help="Steam installation root whose libraryfolders.vdf lists libraries to scan. May be repeated.")
    parser.add_argument("--switch-sd", metavar="ROOT", type=Path,
                        help="Mounted Switch SD card root containing the switch directory.")
    parser.add_argument("--corkdeck-catalog", metavar="FILE", type=Path, action="append", default=[],
                        help="CorkDeck v2/v3 launcher-library-v2.ini to import. May be repeated.")
    parser.add_argument("--output", metavar="FILE", type=Path,
                        help="Write the catalog directly to FILE. Defaults to stdout.")
    args = parser.parse_args()
    if not args.steam_library and not args.steam_install and args.switch_sd is None and not args.corkdeck_catalog:
        parser.error("supply at least one --steam-library, --steam-install, --switch-sd or --corkdeck-catalog")

    steam, steam_errors = scan_libraries(args.steam_library, steam_installs=args.steam_install)
    homebrew, homebrew_errors = scan_homebrew(args.switch_sd) if args.switch_sd else ([], False)
    corkdeck, corkdeck_errors = scan_corkdeck_catalogs(args.corkdeck_catalog)
    entries = steam + homebrew + corkdeck
    try:
        if args.output is None:
            write_catalog(entries)
        else:
            write_catalog_file(args.output, entries)
    except (CatalogLimitError, CatalogOutputError) as exc:
        print(exc, file=sys.stderr)
        return 1
    return int(steam_errors or homebrew_errors or corkdeck_errors)


if __name__ == "__main__":
    sys.exit(main())
