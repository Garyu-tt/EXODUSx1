"""Stage a narrowly patched copy of a pinned FreeBSD source file."""
import argparse
import json
from pathlib import Path

from audit_freebsd import LOCK, blob_id

def stage(checkout, output, module="expand"):
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    if module not in ("expand", "input", "trap", "main", "kill"):
        raise ValueError("unsupported module")
    path = "bin/kill/kill.c" if module == "kill" else "bin/sh/" + module + ".c"
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
    elif module == "kill":
        old = b'\t\t(void)fprintf(fp, "%s", sys_signame[n]);'
        new = b'\t\tif (!nxbsd_signal_name(n)) continue;\n\t\t(void)fprintf(fp, "%s", nxbsd_signal_name(n));'
        if data.count(old) != 1:
            raise ValueError("unexpected kill signal list")
        data = data.replace(old, new)
        for a, b, count in ((b"sys_nsig", b"NSIG", 3),
                            (b"SIG2STR_MAX", b"NXBSD_SIG2STR_MAX", 1),
                            (b"sig2str(", b"nxbsd_sig2str(", 1),
                            (b"str2sig(", b"nxbsd_str2sig(", 2)):
            if data.count(a) != count:
                raise ValueError("unexpected kill adapter anchor")
            data = data.replace(a, b)
        old = new = b""
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
    p.add_argument("--module", choices=("expand", "input", "trap", "main", "kill"), default="expand")
    args = p.parse_args()
    stage(args.checkout, args.out, args.module)
