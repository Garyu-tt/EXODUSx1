"""Verify pinned FreeBSD donor files and print provider dependency order."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

LOCK = Path(__file__).with_name("freebsd-sh-lock.json")
SHA = re.compile(r"[0-9a-f]{40}")

def blob_id(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()

def plan(lock):
    if lock.get("schema") != 1 or not SHA.fullmatch(lock["commit"]):
        raise ValueError("invalid lock version or commit")
    providers = lock["providers"]
    seen = set()
    active = set()
    order = []
    def visit(name):
        if name in active:
            raise ValueError("provider dependency cycle: " + name)
        if name in seen:
            return
        if name not in providers:
            raise ValueError("unknown provider: " + name)
        active.add(name)
        for dep in providers[name]["requires"]:
            visit(dep)
        active.remove(name)
        seen.add(name)
        order.append(name)
    for name in providers:
        visit(name)
    paths = set()
    for entry in lock["files"]:
        p = entry["path"]
        if (not re.fullmatch(r"[A-Za-z0-9_./-]+", p)
                or any(x in ("", ".", "..") for x in p.split("/"))
                or p in paths or not SHA.fullmatch(entry["blob"])
                or entry["capability"] not in providers):
            raise ValueError("invalid donor entry: " + p)
        paths.add(p)
    return order

def verify(root, lock):
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"],
                                   text=True, timeout=10).strip()
    if head != lock["commit"]:
        raise ValueError("checkout HEAD differs from locked commit")
    for entry in lock["files"]:
        p = (root / entry["path"]).resolve(strict=True)
        if root not in p.parents or not p.is_file():
            raise ValueError("unsafe donor path")
        if p.stat().st_size > 4 * 1024 * 1024:
            raise ValueError("donor file exceeds verification limit")
        if blob_id(p.read_bytes()) != entry["blob"]:
            raise ValueError("donor blob mismatch: " + entry["path"])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, help="Existing local pinned FreeBSD source checkout")
    args = parser.parse_args()
    try:
        lock = json.loads(LOCK.read_text(encoding="utf-8"))
        order = plan(lock)
        if args.checkout:
            verify(args.checkout.resolve(strict=True), lock)
        print(json.dumps({"commit": lock["commit"], "files": len(lock["files"]),
                          "source_verified": bool(args.checkout),
                          "native_ready": False,
                          "provider_order": order}, indent=2))
    except (ValueError, OSError, KeyError, subprocess.SubprocessError) as e:
        print(str(e), file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
