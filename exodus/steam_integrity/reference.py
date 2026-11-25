"""Export a file checksum reference from a trusted local Steam installation."""

import argparse
import hashlib
import os
from pathlib import Path
import re
import stat
import sys

from exodus.steam_catalog import FULLY_INSTALLED, optional_uint32, parse_keyvalues


HEADER = b"EXODUS-STEAM-SHA256 1"
SD_COMMON_PREFIX = b"/switch/exodus-steam/steamapps/common/"
MAX_SD_PATH_BYTES = 768
MAX_ACF_BYTES = 64 * 1024
MAX_ACF_DEPTH = 16
MAX_FILES = 32_768
MAX_REFERENCE_BYTES = 8 * 1024 * 1024
MAX_UINT64 = (1 << 64) - 1
READ_BYTES = 1024 * 1024
WINDOWS_DEVICES = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"} | {
    prefix + digit for prefix in ("COM", "LPT") for digit in "123456789¹²³"
}


def _component_bytes(name):
    if not isinstance(name, str) or not name or name.startswith(" ") or name.endswith((" ", ".")):
        raise ValueError(f"unsafe path component: {name!r}")
    if any(char in '/\\:*?"<>|' or ord(char) < 32 or 127 <= ord(char) <= 159 for char in name):
        raise ValueError(f"unsafe path component: {name!r}")
    stem = name.split(".", 1)[0].rstrip(" ").upper()
    if stem in WINDOWS_DEVICES:
        raise ValueError(f"reserved path component: {name!r}")
    encoded = name.encode("utf-8")
    if len(encoded) > 255:
        raise ValueError(f"path component exceeds 255 UTF-8 bytes: {name!r}")
    return encoded


def _is_link(info):
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
    )


def _checked_type(path, is_type, expected):
    info = path.lstat()
    if _is_link(info) or not is_type(info.st_mode):
        raise ValueError(f"{path}: expected a real {expected}, without links or reparse points")
    return info


def _directory_path(path):
    # Keep parent components until their prefixes have been checked. Collapsing
    # "link/.." or "missing/.." first can select a different directory.
    path = Path(path).absolute()
    for part in reversed((path, *path.parents)):
        _checked_type(part, stat.S_ISDIR, "directory")
    return Path(os.path.abspath(path))


def _identity(info):
    return info.st_dev, info.st_ino


def _snapshot(info):
    # Windows path and handle queries can represent creation time and the
    # filename-derived execute bits differently for the same file.
    status_change = info.st_ctime_ns if os.name != "nt" else 0
    return _identity(info), stat.S_IFMT(info.st_mode), info.st_size, info.st_mtime_ns, status_change


def _check_unchanged(path, before, after):
    if _is_link(after) or _snapshot(before) != _snapshot(after):
        raise ValueError(f"{path}: changed while preparing the reference")


def _open_read_only(path):
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    flags |= getattr(os, "O_NONBLOCK", 0)
    return os.fdopen(os.open(path, flags), "rb")


def _read_installation(steamapps, app_id):
    manifest = steamapps / f"appmanifest_{app_id}.acf"
    before = _checked_type(manifest, stat.S_ISREG, "manifest file")
    if before.st_size > MAX_ACF_BYTES:
        raise ValueError(f"{manifest}: manifest exceeds 64 KiB")
    with _open_read_only(manifest) as stream:
        _check_unchanged(manifest, before, os.fstat(stream.fileno()))
        data = stream.read(MAX_ACF_BYTES + 1)
        if len(data) > MAX_ACF_BYTES:
            raise ValueError(f"{manifest}: manifest exceeds 64 KiB")
        if len(data) != before.st_size:
            raise ValueError(f"{manifest}: changed while reading the manifest")
        _check_unchanged(manifest, before, os.fstat(stream.fileno()))
    _check_unchanged(manifest, before, manifest.lstat())
    document = parse_keyvalues(
        data.decode("utf-8-sig"), max_depth=MAX_ACF_DEPTH,
        unique_keys_by_depth={0: {"appstate"}, 1: {"appid", "installdir", "stateflags"}},
    )
    app_state = document.get("appstate")
    if len(document) != 1 or not isinstance(app_state, dict):
        raise ValueError(f"{manifest}: expected one AppState object")
    if app_state.get("appid") != app_id:
        raise ValueError(f"{manifest}: App ID does not match the filename and request")
    flags = optional_uint32(app_state, "stateflags")
    if flags is None or not flags & FULLY_INSTALLED:
        raise ValueError(f"{manifest}: Steam does not report a fully installed game")
    directory = app_state.get("installdir")
    max_relative = MAX_SD_PATH_BYTES - len(SD_COMMON_PREFIX) - len(_component_bytes(directory)) - 1
    path = _directory_path(steamapps / "common" / directory)
    return path, manifest, before, max_relative


def _header(app_id, count):
    return HEADER + f"\n{app_id}\n{count}\n".encode("ascii")


def _inventory(game, app_id, max_relative):
    files = {}
    total_size = 0
    payload_size = 0

    def visit(directory, prefix, before):
        nonlocal total_size, payload_size
        _check_unchanged(directory, before, directory.lstat())
        sibling_names = set()
        with os.scandir(directory) as entries:
            for entry in entries:
                component = _component_bytes(entry.name)
                folded_name = entry.name.casefold()
                if folded_name in sibling_names:
                    raise ValueError(f"{directory}: case-alias path component {entry.name!r}")
                sibling_names.add(folded_name)
                path = Path(entry.path)
                relative = prefix + component
                if len(relative) > max_relative:
                    raise ValueError(f"{path}: full Switch SD path exceeds {MAX_SD_PATH_BYTES} UTF-8 bytes")
                info = path.lstat()
                if _is_link(info):
                    raise ValueError(f"{path}: links and reparse points cannot be exported")
                if stat.S_ISDIR(info.st_mode):
                    visit(path, relative + b"/", info)
                    continue
                if not stat.S_ISREG(info.st_mode):
                    raise ValueError(f"{path}: expected a regular game file")
                count = len(files) + 1
                if count > MAX_FILES:
                    raise ValueError(f"reference exceeds {MAX_FILES} files")
                if info.st_size < 0 or info.st_size > MAX_UINT64 - total_size:
                    raise ValueError("total game file size exceeds the uint64 range")
                total_size += info.st_size
                payload_size += 67 + len(str(info.st_size)) + len(relative)
                if payload_size + len(_header(app_id, count)) > MAX_REFERENCE_BYTES:
                    raise ValueError(f"reference exceeds {MAX_REFERENCE_BYTES} bytes")
                files[relative] = (path, info)
        _check_unchanged(directory, before, directory.lstat())

    visit(game, b"", _checked_type(game, stat.S_ISDIR, "game directory"))
    if not files:
        raise ValueError(f"{game}: game directory has no regular files")
    return files


def _hash_file(path, before):
    _check_unchanged(path, before, path.lstat())
    digest = hashlib.sha256()
    remaining = before.st_size
    with _open_read_only(path) as stream:
        _check_unchanged(path, before, os.fstat(stream.fileno()))
        while remaining:
            block = stream.read(min(READ_BYTES, remaining))
            if not block:
                raise ValueError(f"{path}: became shorter while hashing")
            digest.update(block)
            remaining -= len(block)
        if stream.read(1):
            raise ValueError(f"{path}: became larger while hashing")
        _check_unchanged(path, before, os.fstat(stream.fileno()))
    _check_unchanged(path, before, path.lstat())
    return digest.hexdigest().encode("ascii")


def _write_new_reference(output, payload):
    created = None
    try:
        with output.open("xb") as stream:
            created = os.fstat(stream.fileno())
            stream.write(payload)
    except BaseException:
        if created is not None:
            try:
                current = output.lstat()
                if not _is_link(current) and _identity(current) == _identity(created):
                    output.unlink()
            except OSError:
                pass
        raise


def export_reference(steamapps: Path, app_id: str, output: Path) -> str:
    """Create a new reference outside the game directory and return its SHA-256."""
    if not re.fullmatch(r"[1-9][0-9]{0,9}", app_id) or int(app_id) > 0xFFFFFFFF:
        raise ValueError("App ID must be a canonical positive uint32")
    game, manifest, manifest_before, max_relative = _read_installation(_directory_path(steamapps), app_id)
    _component_bytes(output.name)
    game_root = game.resolve(strict=True)
    output_parent = _directory_path(output.parent).resolve(strict=True)
    output = output_parent / output.name
    if game_root == output_parent or game_root in output_parent.parents:
        raise ValueError("reference output must be outside the game directory")
    if os.path.lexists(output):
        raise FileExistsError(f"reference already exists: {output}")
    game_before = game.lstat()
    output_parent_before = output_parent.lstat()

    files = _inventory(game, app_id, max_relative)
    payload = bytearray(_header(app_id, len(files)))
    for relative in sorted(files, key=bytes.lower):
        path, info = files[relative]
        payload.extend(_hash_file(path, info))
        payload.extend(b"\t" + str(info.st_size).encode("ascii") + b"\t" + relative + b"\n")

    current = _inventory(game, app_id, max_relative)
    if files.keys() != current.keys():
        raise ValueError(f"{game}: game file list changed while preparing the reference")
    for relative, (path, info) in files.items():
        _check_unchanged(path, info, current[relative][1])
    _check_unchanged(game, game_before, game.lstat())
    _check_unchanged(manifest, manifest_before, manifest.lstat())
    _directory_path(output_parent)
    if _identity(output_parent_before) != _identity(output_parent.lstat()):
        raise ValueError(f"{output_parent}: output directory changed while preparing the reference")
    _write_new_reference(output, payload)
    return hashlib.sha256(payload).hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--steamapps", type=Path, required=True, help="trusted Steam library's steamapps directory")
    parser.add_argument("--app-id", required=True, help="canonical decimal Steam App ID")
    parser.add_argument("--output", type=Path, required=True, help="new reference file outside the game directory")
    args = parser.parse_args(argv)
    try:
        digest = export_reference(args.steamapps, args.app_id, args.output)
    except (OSError, ValueError) as exc:
        print(f"Steam file reference: {exc}", file=sys.stderr)
        return 1
    print(digest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
