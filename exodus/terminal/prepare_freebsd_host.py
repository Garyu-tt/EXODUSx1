"""Stage a narrowly patched copy of a pinned FreeBSD source file."""
import argparse
import json
from pathlib import Path

from audit_freebsd import LOCK, blob_id

def stage(checkout, output, module="expand"):
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    if module not in ("expand", "input"):
        raise ValueError("unsupported module")
    path = "bin/sh/" + module + ".c"
    expected = next(x["blob"] for x in lock["files"] if x["path"] == path)
    data = (checkout / path).read_bytes()
    if blob_id(data) != expected:
        raise ValueError("unexpected donor blob")
    if module == "expand":
        old = b"namlen = dp->d_namlen;"
        new = b"namlen = strlen(dp->d_name);"
    else:
        # O_VERIFY is a security request, not a cosmetic file flag.
        # A host without FreeBSD verification must refuse verified opens.
        old = b"oflags |= O_VERIFY;"
        new = b'errorwithstatus(126, "verified script open unavailable on host");'
    if data.count(old) != 1:
        raise ValueError("unexpected portability patch anchor")
    # Preserve the original source and its license in the staged copy.
    with output.open("xb") as f:
        f.write(data.replace(old, new))

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--checkout", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--module", choices=("expand", "input"), default="expand")
    args = p.parse_args()
    stage(args.checkout, args.out, args.module)
