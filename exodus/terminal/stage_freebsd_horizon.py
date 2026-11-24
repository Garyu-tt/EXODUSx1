"""Copy only locked FreeBSD sources; patch target-specific portability anchors."""
import argparse
import json
from pathlib import Path
from audit_freebsd import LOCK, verify
from prepare_freebsd_host import stage

def prepare(donor, out):
    donor = donor.resolve(strict=True)
    lock = json.loads(LOCK.read_text())
    verify(donor, lock)
    out.mkdir()
    for entry in lock["files"]:
        path = entry["path"]
        dst = out / path
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes((donor / path).read_bytes())

    def replace(path, old, new):
        dst = out / path
        data = dst.read_bytes()
        if data.count(old) != 1:
            raise ValueError("unexpected target patch anchor: " + path)
        dst.write_bytes(data.replace(old, new))

    # Use target libc's standard nonlocal jump primitives. Signal-mask
    # compatibility remains unverified; this does not enable job control.
    replace("bin/sh/error.h", b"#define setjmp(jmploc)\t_setjmp(jmploc)",
            b"/* target: standard setjmp */")
    replace("bin/sh/error.h", b"#define longjmp(jmploc, val)\t_longjmp(jmploc, val)",
            b"/* target: standard longjmp */")
    for module in ("expand", "input", "main"):
        dst = out / "bin/sh" / (module + ".c")
        dst.unlink()
        stage(donor, dst, module)
    for module in ("input", "main"):
        dst = out / "bin/sh" / (module + ".c")
        data = dst.read_bytes()
        if data.count(b"unavailable on host") != 1:
            raise ValueError("unexpected verified-open refusal")
        dst.write_bytes(data.replace(b"unavailable on host", b"unavailable on Horizon"))
    # In SHELL builds the donor's bltin.h already supplies warning/error
    # wrappers; the standalone err.h API is not required.
    for path in ("usr.bin/printf/printf.c", "bin/test/test.c", "bin/kill/kill.c"):
        replace(path, b"#include <err.h>",
                b"#ifndef SHELL\n#include <err.h>\n#endif")
    # Reuse the tested AST bridge hook; no target subprocess fallback is
    # admitted for bridged pipelines. Non-pipeline paths still require review.
    dst = out / "bin/sh/eval.c"
    dst.unlink()
    stage(donor, dst, "evalbridge")
    data = dst.read_bytes()
    if data.count(b"_PATH_STDPATH") != 2:
        raise ValueError("unexpected standard PATH anchor")
    dst.write_bytes(data.replace(b"_PATH_STDPATH", b'"/switch"'))
    # Explicit virtual path policy; not a claim that PATH executables work.
    replace("bin/sh/var.c", b'"PATH=" _PATH_DEFPATH', b'"PATH=/switch"')
    replace("bin/sh/output.c", b"fwopen(dest, doformat_wr)",
            b"nxbsd_fwopen(dest, doformat_wr)")
    replace("bin/sh/output.c", b"fwopen(out1, doformat_wr)",
            b"nxbsd_fwopen(out1, doformat_wr)")

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--checkout", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    a = p.parse_args()
    prepare(a.checkout, a.out)
