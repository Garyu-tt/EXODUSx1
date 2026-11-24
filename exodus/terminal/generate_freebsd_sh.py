"""Build original FreeBSD sh generators from the verified donor checkout.
This is a host build stage, not a compiled parser or target shell.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from audit_freebsd import LOCK, plan, verify

GENERATED = ("nodes.c", "nodes.h", "syntax.c", "syntax.h",
             "builtins.c", "builtins.h", "token.h")

def generate(checkout, out):
    root = checkout.resolve(strict=True)
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    plan(lock)
    verify(root, lock)
    source = root / "bin/sh"
    # Upstream mkbuiltins assumes a whitespace-free source path.
    # Stage only verified files in a private temporary directory.
    with tempfile.TemporaryDirectory(prefix="nxbsd-") as tmp:
        work = Path(tmp)
        src = work / "src"
        src.mkdir()
        names = ("mknodes.c", "mksyntax.c", "mkbuiltins", "mktokens",
                 "nodetypes", "nodes.c.pat", "builtins.def", "parser.h", "shell.h")
        entries = {x["path"]: x["blob"] for x in lock["files"]}
        from audit_freebsd import blob_id
        for name in names:
            data = (source / name).read_bytes()
            if blob_id(data) != entries["bin/sh/" + name]:
                raise ValueError("generator input changed: " + name)
            (src / name).write_bytes(data)
        # Host-only annotation shim, not a target libc compatibility shim.
        shim = work / "host_annotations.h"
        shim.write_text(
            "#include <sys/cdefs.h>\n"
            "#ifndef __printf0like\n#define __printf0like(a,b) __attribute__((format(printf,a,b)))\n#endif\n"
            "#ifndef __dead2\n#define __dead2 __attribute__((noreturn))\n#endif\n"
            "#ifndef __unused\n#define __unused __attribute__((unused))\n#endif\n",
            encoding="ascii")
        cc = os.environ.get("CC", "cc")
        env = dict(os.environ, LC_ALL="C", TMPDIR=str(work))
        def run(args):
            subprocess.run(args, cwd=work, env=env, check=True, timeout=60)
        for name in ("mknodes", "mksyntax"):
            run([cc, "-std=gnu11", "-O2", "-include", str(shim),
                 str(src / (name + ".c")), "-o", str(work / name)])
        run([str(work / "mknodes"), str(src / "nodetypes"), str(src / "nodes.c.pat")])
        run([str(work / "mksyntax")])
        run(["sh", str(src / "mkbuiltins"), "src"])
        run(["sh", str(src / "mktokens")])
        required = {"nodes.h": ("NCMD", "NPIPE", "union node"),
                    "syntax.h": ("BASESYNTAX", "CWORD"),
                    "builtins.h": ("builtinfunc", "CDCMD"),
                    "token.h": ("TPIPE", "TIF", "TEOF")}
        report = {}
        for name in GENERATED:
            data = (work / name).read_bytes()
            if not data or len(data) > 2 * 1024 * 1024:
                raise ValueError("invalid generated output: " + name)
            if any(marker.encode() not in data for marker in required.get(name, ())):
                raise ValueError("missing generated marker: " + name)
            report[name] = hashlib.sha256(data).hexdigest()
        # Require a new output path, never overwrite existing artifacts.
        out.mkdir(parents=False, exist_ok=False)
        for name in GENERATED:
            shutil.copyfile(work / name, out / name)
        (out / "manifest.json").write_text(json.dumps({
            "upstream": lock["commit"], "files_sha256": report,
            "stage": "host_generators_only", "native_shell_ready": False
        }, indent=2) + "\n", encoding="utf-8")
        return report

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--checkout", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    print(json.dumps(generate(args.checkout, args.out.resolve()), indent=2))

if __name__ == "__main__":
    main()
