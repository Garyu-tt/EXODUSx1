"""Exercise the linked FreeBSD shell on Linux; not a Horizon test."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SHELL = None
if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("usage: test_freebsd_shell_host.py /path/to/nxbsd-sh")
    SHELL = Path(sys.argv.pop(1)).resolve()

@unittest.skipIf(SHELL is None, "requires the linked FreeBSD shell executable")
class ShellTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.cwd = Path(self.tmp.name)

    def run_shell(self, script, *opts):
        return subprocess.run([str(SHELL), *opts, "-c", script],
                              cwd=self.cwd, text=True, capture_output=True,
                              timeout=8, env={"PATH": "/usr/bin:/bin",
                              "HOME": str(self.cwd), "LC_ALL": "C"})

    def check(self, script, expected, status=0):
        p = self.run_shell(script)
        self.assertEqual(p.returncode, status, p.stderr)
        self.assertEqual(p.stdout, expected)

    def test_expansion(self):
        self.check('x=hello; printf "%s %s\\n" "$x" "$((6*7))"', "hello 42\n")

    def test_control(self):
        self.check('for x in a b; do echo "$x"; done; if false; then echo bad; else echo ok; fi',
                   "a\nb\nok\n")

    def test_pipeline_subshell(self):
        self.check('printf "abc\\n" | tr a-z A-Z; x=outer; (x=inner; echo "$x"); echo "$x"',
                   "ABC\ninner\nouter\n")

    def test_files(self):
        self.check('printf "%s\\n" hello > a.nro; test -f a.nro && cat a.nro; printf "%s\\n" *.nro',
                   "hello\na.nro\n")

    def test_async_wait(self):
        self.check('false & p=$!; wait "$p"; printf "%s\n" "$?"', "1\n")

    def test_command_substitution(self):
        self.check('x=$(printf "%s\n" abc); printf "<%s>\n" "$x"', "<abc>\n")

    def test_function_scope(self):
        self.check('x=outer; f() { local x=inner; echo "$x"; }; f; echo "$x"',
                   "inner\nouter\n")

    def test_descriptor_duplication(self):
        self.check('echo text 3>out 1>&3; cat out', "text\n")

    def test_trap(self):
        self.check('trap "echo cleanup" EXIT; echo body', "body\ncleanup\n")

    def test_status(self):
        self.check("false", "", 1)
        self.check("exit 7", "", 7)

    def test_verified_script_refused(self):
        (self.cwd / "script.sh").write_text("echo executed\n")
        self.check(". ./script.sh", "executed\n")
        p = self.run_shell(". ./script.sh", "-o", "verify")
        self.assertNotEqual(p.returncode, 0)
        self.assertNotIn("executed", p.stdout)
        self.assertIn("verified script open unavailable", p.stderr)

if __name__ == "__main__":
    unittest.main()
