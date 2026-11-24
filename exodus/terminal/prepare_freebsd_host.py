"""Stage a narrowly patched copy of a pinned FreeBSD source file."""
import argparse
import json
from pathlib import Path

from audit_freebsd import LOCK, blob_id

def stage(checkout, output):
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    path = "bin/sh/expand.c"
    expected = next(x["blob"] for x in lock["files"] if x["path"] == path)
    data = (checkout / path).read_bytes()
    if blob_id(data) != expected:
        raise ValueError("unexpected donor blob")
    old = b"namlen = dp->d_namlen;"
    new = b"namlen = strlen(dp->d_name);"
    if data.count(old) != 1:
        raise ValueError("unexpected directory length expression")
    # readdir guarantees a NUL-terminated d_name; BSD d_namlen excludes NUL.
    # Preserve the original source and its license in the staged copy.
    with output.open("xb") as f:
        f.write(data.replace(old, new))

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--checkout", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    args = p.parse_args()
    stage(args.checkout, args.out)
