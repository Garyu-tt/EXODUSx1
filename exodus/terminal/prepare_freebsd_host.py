"""Stage a narrowly patched copy of a pinned FreeBSD source file."""
import argparse
import json
from pathlib import Path

from audit_freebsd import LOCK, blob_id

def stage(checkout, output, module="expand"):
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    if module not in ("expand", "input", "trap", "main"):
        raise ValueError("unsupported module")
    path = "bin/sh/" + module + ".c"
    expected = next(x["blob"] for x in lock["files"] if x["path"] == path)
    data = (checkout / path).read_bytes()
    if blob_id(data) != expected:
        raise ValueError("unexpected donor blob")
    if module == "expand":
        old = b"namlen = dp->d_namlen;"
        new = b"namlen = strlen(dp->d_name);"
    elif module == "input":
        # O_VERIFY is a security request, not a cosmetic file flag.
        # A host without FreeBSD verification must refuse verified opens.
        old = b"oflags |= O_VERIFY;"
        new = b'errorwithstatus(126, "verified script open unavailable on host");'
    elif module == "main":
        # A verified profile must not be executed without verification.
        old = b"oflags |= O_VERIFY;"
        new = b'errorwithstatus(126, "verified profile open unavailable on host");'
    else:
        # Host signal numbers and abbreviations must come from host libc.
        if data.count(b"sys_nsig") != 4 or data.count(b"sys_signame[n]") != 5 or data.count(b"sys_signame[signo]") != 2:
            raise ValueError("unexpected signal table references")
        data = data.replace(b"sys_nsig", b"NSIG").replace(b"sys_signame[n]", b"nxbsd_signal_name(n)").replace(b"sys_signame[signo]", b"nxbsd_signal_name(signo)")
        old = new = b""
    if old and data.count(old) != 1:
        raise ValueError("unexpected portability patch anchor")
    # Preserve the original source and its license in the staged copy.
    with output.open("xb") as f:
        f.write(data.replace(old, new) if old else data)

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--checkout", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--module", choices=("expand", "input", "trap", "main"), default="expand")
    args = p.parse_args()
    stage(args.checkout, args.out, args.module)
