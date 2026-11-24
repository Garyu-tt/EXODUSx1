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
    # The upstream shell has an explicit no-Berkeley-job-control build.
    # Use it instead of inventing a termios ABI or pretending to own a tty.
    replace("bin/sh/shell.h", b"#define\tJOBS 1", b"#define\tJOBS 0")
    replace("bin/sh/main.c", b"#include <termios.h>\n", b"")
    replace("bin/sh/main.c", b"\tif (rootpid == 1) {",
            b"#ifndef __SWITCH__\n\tif (rootpid == 1) {")
    replace("bin/sh/main.c", b"\t}\n\trootshell = 1;",
            b"\t}\n#endif\n\trootshell = 1;")
    # Effective UID/GID and faccessat are unavailable at target link.
    # Refuse permission predicates explicitly instead of forging credentials.
    replace("bin/test/test.c",
            b"\tcase FILRD:\n\t\treturn (eaccess(nm, R_OK) == 0);\n\tcase FILWR:\n\t\treturn (eaccess(nm, W_OK) == 0);\n\tcase FILEX:\n\t\t/* XXX work around eaccess(2) false positives for superuser */\n\t\tif (eaccess(nm, X_OK) != 0)\n\t\t\treturn 0;\n\t\tif (S_ISDIR(s.st_mode) || geteuid() != 0)\n\t\t\treturn 1;\n\t\treturn (s.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;\n\tcase FILEXIST:\n\t\treturn (eaccess(nm, F_OK) == 0);",
            b"\tcase FILRD:\n\tcase FILWR:\n\tcase FILEX:\n\t\terror(\"effective access checks unavailable on Horizon\");\n\tcase FILEXIST:\n\t\treturn 1; /* stat above succeeded */")
    replace("bin/sh/exec.c", b"if (eaccess(argv[i], X_OK) == 0) {",
            b"if ((errno = ENOTSUP, 0)) { /* no effective access provider */")
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
