"""Read-only EVCS import preflight; never installs, moves, or deletes content."""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat

MAX_PARTS = 100
FAT32_MAX = (1 << 32) - 1


def is_indirect(info):
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0) &
        getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def direct_path(value):
    path = Path(value)
    # Reject parent traversal before any normalization can hide a symlink.
    if ".." in path.parts or (path.drive and not path.is_absolute()):
        raise ValueError("parent traversal or drive-relative path not supported")
    path = path.absolute()
    for component in (*reversed(path.parents), path):
        if is_indirect(component.lstat()):
            raise ValueError("symlink or reparse point not supported")
    if path.resolve(strict=True) != path:
        raise ValueError("indirect path not supported")
    return path


def regular(path):
    info = path.lstat()
    if is_indirect(info):
        raise ValueError(f"indirect file: {path}")
    if not stat.S_ISREG(info.st_mode) or info.st_size <= 0:
        raise ValueError(f"not a nonempty regular file: {path}")
    return info


def inventory(source):
    if source.suffix.lower() not in {".nro", ".nsp", ".xci"}:
        raise ValueError("unsupported source extension")
    info = source.lstat()
    if is_indirect(info):
        raise ValueError("indirect source not supported")
    if stat.S_ISREG(info.st_mode):
        return [regular(source)]
    if not stat.S_ISDIR(info.st_mode) or source.suffix.lower() == ".nro":
        raise ValueError("unsupported source type")
    parts = {}
    with os.scandir(source) as entries:
        for item in entries:
            name = item.name
            if len(name) != 2 or any(c not in "0123456789" for c in name):
                raise ValueError("unexpected split directory member")
            part = regular(source / name)
            if part.st_size > FAT32_MAX:
                raise ValueError("part exceeds FAT32 limit")
            parts[int(name)] = part
    if not 2 <= len(parts) <= MAX_PARTS or set(parts) != set(range(len(parts))):
        raise ValueError("split parts missing or not contiguous")
    ordered = [parts[i] for i in range(len(parts))]
    first = ordered[0].st_size
    if any(p.st_size != first for p in ordered[:-1]) or ordered[-1].st_size > first:
        raise ValueError("inconsistent split sizes")
    return ordered


def plan(source, games, fat32=False):
    source = direct_path(source)
    games = direct_path(games)
    root = games.lstat()
    if not stat.S_ISDIR(root.st_mode):
        raise ValueError("Games root must exist and be a directory")
    if source == games or games.is_relative_to(source):
        raise ValueError("source contains Games root")
    parts = inventory(source)
    total = sum(p.st_size for p in parts)
    allocated = (sum(p.st_blocks * 512 for p in parts)
                 if all(hasattr(p, "st_blocks") for p in parts) else None)
    free = shutil.disk_usage(games).free
    same_device = all(p.st_dev == root.st_dev for p in parts)
    already = source.is_relative_to(games)
    if already:
        status = "already_discoverable"
        destination = source
    else:
        destination = games / source.name
        with os.scandir(games) as entries:
            collision = any(e.name.casefold() == source.name.casefold() for e in entries)
        if collision:
            status = "destination_exists"
        elif fat32 and any(p.st_size > FAT32_MAX for p in parts):
            status = "fat32_file_too_large"
        elif free < total:
            status = "insufficient_copy_space"
        else:
            status = "copy_preflight_ok"
    return {"status": status, "source": str(source), "destination": str(destination),
            "parts": len(parts), "logical_bytes": total, "source_allocated_bytes": allocated,
            "free_bytes": free, "minimum_copy_payload_bytes": 0 if already else total,
            "same_device": same_device, "fat32_requested": fat32,
            "launch_authorized": False, "writes_performed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("games", type=Path)
    parser.add_argument("--fat32", action="store_true")
    args = parser.parse_args()
    try:
        report = plan(args.source, args.games, args.fat32)
    except (OSError, ValueError) as exc:
        parser.exit(2, f"preflight failed: {exc}\n")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] in {"already_discoverable", "copy_preflight_ok"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
