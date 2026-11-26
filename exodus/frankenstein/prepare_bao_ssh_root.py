"""Stage a bounded, QEMU-only OpenSSH root from a verified FreeBSD base tree.

Only copy named executables, configuration and ELF DT_NEEDED closure.
No execution of guest executables on the build host.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

PROGRAMS = (
    "usr/sbin/sshd", "usr/sbin/pwd_mkdb",
    "usr/bin/ssh-keygen", "usr/bin/ssh-keyscan", "usr/bin/ssh",
    "usr/libexec/sshd-session", "usr/libexec/sshd-auth",
    "libexec/ld-elf.so.1",
)
CONFIG = ("etc/master.passwd", "etc/group", "etc/services",
          "etc/nsswitch.conf", "etc/login.conf")
LIBDIRS = ("lib", "usr/lib", "usr/lib/private")
MAX_BYTES = 52 * 1024 * 1024


def stage(base, root):
    base, root = base.resolve(strict=True), root.resolve(strict=True)
    if base == root or base in root.parents or root in base.parents:
        raise ValueError("base and root must be disjoint")
    seen = set()
    planned = []
    queue = list(PROGRAMS + CONFIG)
    # The rescue binary has several hard links: account for its inode once.
    inodes = set()
    total = 0
    for p in root.rglob("*"):
        if p.is_file():
            s = p.stat()
            key = (s.st_dev, s.st_ino)
            if key not in inodes:
                inodes.add(key)
                total += s.st_size
    while queue:
        rel = queue.pop(0)
        if rel in seen:
            continue
        if len(seen) >= 256:
            raise ValueError("dependency count exceeded")
        source = (base / rel).resolve(strict=True)
        if not source.is_relative_to(base) or not source.is_file():
            raise ValueError("source escapes base: " + rel)
        target = root / rel
        if (not target.parent.resolve().is_relative_to(root)
                or any(parent.is_symlink() for parent in target.parents if parent != root)
                or target.exists() or target.is_symlink()):
            raise ValueError("destination not fresh: " + rel)
        total += source.stat().st_size
        if total > MAX_BYTES:
            raise ValueError("staged files exceed 52 MiB")
        planned.append((source, target))
        seen.add(rel)
        with source.open("rb") as fp:
            if fp.read(4) != b"\x7fELF":
                if rel not in CONFIG:
                    raise ValueError("required executable or dependency is not ELF: " + rel)
                continue
        output = subprocess.check_output(
            ["readelf", "-h", "-d", str(source)], text=True,
            env={**os.environ, "LC_ALL": "C"}, timeout=10)
        if not re.search(r"Machine:\s+AArch64", output):
            raise ValueError("non-AArch64 ELF: " + rel)
        for name in re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", output):
            if not re.fullmatch(r"[A-Za-z0-9_.+-]+", name):
                raise ValueError("unexpected dependency name")
            candidates = [d + "/" + name for d in LIBDIRS
                          if (base / d / name).is_file()]
            if not candidates:
                raise ValueError("missing DT_NEEDED: " + name)
            queue.append(candidates[0])
    # Do not mutate the root until the complete dependency closure is admitted.
    for source, target in planned:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    print("Staged", len(seen), "paths;", total, "unique-file bytes")
    for rel in sorted(seen):
        print(rel)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: prepare_bao_ssh_root.py verified-base stock-root-copy")
    stage(Path(sys.argv[1]), Path(sys.argv[2]))
