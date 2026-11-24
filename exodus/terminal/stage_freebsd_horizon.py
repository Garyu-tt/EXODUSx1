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
    # Headless first release: no kernel subprocesses or POSIX signal masks.
    # Replace complete, hash-verified function bodies, not syscall results.
    def body(path, signature, replacement):
        dst = out / path
        data = dst.read_bytes()
        anchor = signature + b"\n{\n"
        if data.count(anchor) != 1:
            raise ValueError("unexpected function anchor: " + path + ": " + repr(signature))
        start = data.index(anchor) + len(anchor)
        end = data.index(b"\n}\n", start)
        dst.write_bytes(data[:start] + replacement + data[end:])

    jobs = "bin/sh/jobs.c"
    replace(jobs, b"\t\t\t\t\tdeljob(job);",
            b"#if JOBS\n\t\t\t\t\tdeljob(job);\n#endif")
    replace(jobs, b"\t\tif (WCOREDUMP(status))\n\t\t\tcoredump = \" (core dumped)\";",
            b"\t\t/* target has no core dump status */")
    body(jobs, b"forkshell(struct job *jp, union node *n, int mode)",
         b"\t(void)jp; (void)n; (void)mode;\n\terrorwithstatus(125, \"native subprocess unavailable on Horizon\");")
    body(jobs, b"killjob(const char *name, int sig)",
         b"\t(void)name; (void)sig; errno = ENOTSUP; return -1;")
    body(jobs, b"dowait(int mode, struct job *job)",
         b"\t(void)mode; (void)job; errno = ECHILD; return -1;")
    # Native task completion uses the separate NX-BSD scheduler. Legacy
    # process job creation is deliberately not admitted.
    body(jobs, b"makejob(union node *node __unused, int nprocs)",
         b"\t(void)node; (void)nprocs;\n\terrorwithstatus(125, \"legacy process job unavailable on Horizon\");")
    # Timed read needs a real interrupt-aware provider; plain read is retained.
    misc = "bin/sh/miscbltin.c"
    dst = out / misc
    data = dst.read_bytes()
    start = data.index(b"\tif (timeout >= 0) {")
    end = data.index(b"\n\tstatus = 0;\n\tstartword", start)
    dst.write_bytes(data[:start] + b"\tif (timeout >= 0)\n\t\terrorwithstatus(125, \"timed read unavailable on Horizon\");\n" + data[end:])
    body(misc, b"umaskcmd(int argc __unused, char **argv __unused)",
         b"\t(void)argc; (void)argv;\n\terrorwithstatus(125, \"umask unavailable on Horizon\");")
    # The resource-limit table and printer are unreachable in this profile.
    # Omit them rather than synthesizing an unsupported rlimit ABI.
    replace(misc, b"struct limits {", b"#ifndef __SWITCH__\nstruct limits {")
    replace(misc, b"\nint\nulimitcmd(int argc __unused, char **argv __unused)", b"\n#endif\nint\nulimitcmd(int argc __unused, char **argv __unused)")
    body(misc, b"ulimitcmd(int argc __unused, char **argv __unused)",
         b"\t(void)argc; (void)argv;\n\terrorwithstatus(125, \"ulimit unavailable on Horizon\");")
    body(jobs, b"vforkexecshell(struct job *jp, char **argv, char **envp, const char *path, int idx, int pip[2])",
         b"\t(void)jp; (void)argv; (void)envp; (void)path; (void)idx; (void)pip;\n\terrorwithstatus(125, \"native external executable unavailable on Horizon\");")
    trap = "bin/sh/trap.c"
    replace(trap, b'#include "shell.h"', b'#include <errno.h>\n#include "shell.h"')
    body(trap, b"sigstring_to_signum(char *sig)",
         b"\tif (!strcasecmp(sig, \"EXIT\") || !strcmp(sig, \"0\")) return 0;\n\treturn -1;")
    body(trap, b"printsignals(void)",
         b"\tout1str(\"EXIT\\n\");")
    replace(trap, b"for (signo = 0 ; signo < sys_nsig ; signo++) {",
            b"for (signo = 0 ; signo < 1 ; signo++) {")
    replace(trap, b"\t\t\t\t} else if (sys_signame[signo]) {\n\t\t\t\t\tout1fmt(\" %s\\n\", sys_signame[signo]);",
            b"\t\t\t\t} else if (0) {\n\t\t\t\t\t/* only EXIT is supported */")
    body(trap, b"setsignal(int signo)",
         b"\t/* Internal startup hook only: no kernel signal handlers installed. */\n\t(void)signo;")
    body(trap, b"getsigaction(int signo, sig_t *sigact)",
         b"\t(void)signo; (void)sigact; errno = ENOTSUP; return 0;")
    body(trap, b"ignoresig(int signo)",
         b"\t(void)signo; /* no kernel signal manipulation */")
    dst = out / trap
    data = dst.read_bytes()
    start = data.index(b"\tif (sig != 0 && sig != SIGSTOP")
    end = data.index(b"\n\t_exit(exiting_exitstatus);", start)
    dst.write_bytes(data[:start] + data[end:])
    # Preserve explicit refusal instead of listing or delivering fake signals.
    kill = "bin/kill/kill.c"
    body(kill, b"main(int argc, char *argv[])",
         b"\t(void)argc; (void)argv;\n\terrorwithstatus(125, \"kernel signal delivery unavailable on Horizon\");")
    body(kill, b"printsignals(FILE *fp)",
         b"\t(void)fp;")
    # Final link admission: no invented identities or external process ABI.
    parser = "bin/sh/parser.c"
    body(parser, b"getpwlogin(void)", b"\treturn NULL; /* no login database */")
    body(parser, b"getusername(char *name, size_t namelen)",
         b"\tif (namelen) strlcpy(name, \"unknown\", namelen);")
    replace(parser, b"ps[i] = (geteuid() != 0) ? '$' : '#';", b"ps[i] = '$';")
    replace("bin/sh/var.c", b'geteuid() ? "PS1=$ " : "PS1=# "', b'"PS1=$ "')
    replace("bin/sh/var.c", b'\tfmtstr(ppid, sizeof(ppid), "%d", (int)getppid());\n\tsetvarsafe("PPID", ppid, 0);',
            b'\t/* PPID unavailable: do not invent a parent process. */')
    replace("bin/sh/expand.c", b"pw = getpwnam(user);\n\t\t\t\thome = pw != NULL ? pw->pw_dir : NULL;",
            b"home = NULL; /* named home lookup unsupported */")
    options = "bin/sh/options.c"
    replace(options, b"privileged = (getuid() != geteuid() || getgid() != getegid());",
            b"privileged = 0; /* no Unix identity transition model */")
    replace(options, b'\tif (&optval[idx] == &privileged && !val && privileged) {\n\t\tif (setgid(getgid()) == -1)\n\t\t\terror("setgid");\n\t\tif (setuid(getuid()) == -1)\n\t\t\terror("setuid");\n\t}',
            b'\tif (&optval[idx] == &privileged && val)\n\t\terrorwithstatus(125, "privileged mode unavailable on Horizon");')
    replace("bin/test/test.c", b"\tcase FILUID:\n\t\treturn s.st_uid == geteuid();\n\tcase FILGID:\n\t\treturn s.st_gid == getegid();",
            b'\tcase FILUID:\n\tcase FILGID:\n\t\terrorwithstatus(125, "Unix ownership unavailable on Horizon");')
    body("bin/sh/exec.c", b"tryexec(char *cmd, char **argv, char **envp)",
         b"\t(void)cmd; (void)argv; (void)envp; errno = ENOTSUP;")
    body("bin/sh/eval.c", b"evalbackcmd(union node *n, struct backcmd *result)",
         b'\t(void)n; (void)result;\n\terrorwithstatus(125, "command substitution unavailable on Horizon");')
    body("bin/sh/eval.c", b"timescmd(int argc __unused, char **argv __unused)",
         b'\t(void)argc; (void)argv;\n\terrorwithstatus(125, "process accounting unavailable on Horizon");')
    # The one remaining legacy pipe in evalcommand is unreachable after
    # makejob refuses; make that branch itself explicitly fail closed.
    replace("bin/sh/eval.c", b'\t\t\tif (pipe(pip) < 0)\n\t\t\t\terror("Pipe call failed: %s", strerror(errno));',
            b'\t\t\terrorwithstatus(125, "legacy process pipe unavailable on Horizon");')
    body("bin/sh/redir.c", b"openhere(union node *redir)",
         b'\t(void)redir;\n\terrorwithstatus(125, "here document unavailable on Horizon");')
    body("bin/sh/error.c", b"onint(void)",
         b'\tintpending = 0;\n\texraise(EXINT);')
    # Generated builtin table still contains fg/bg in this inventory.
    # Supply explicit errors, not a fictitious job-control implementation.
    dst = out / jobs
    with dst.open("ab") as f:
        f.write(b'\n#if !JOBS\nint fgcmd(int argc, char **argv) { (void)argc; (void)argv; errorwithstatus(125, "job control unavailable on Horizon"); }\nint bgcmd(int argc, char **argv) { (void)argc; (void)argv; errorwithstatus(125, "job control unavailable on Horizon"); }\n#endif\n')
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
