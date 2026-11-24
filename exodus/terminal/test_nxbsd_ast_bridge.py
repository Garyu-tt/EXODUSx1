"""Host-only AST bridge integration; ordinary shell commands retain host semantics."""
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest

SHELL = None
if __name__ == "__main__":
    SHELL = Path(sys.argv.pop(1)).resolve()

@unittest.skipIf(SHELL is None, "requires bridge executable")
class BridgeTests(unittest.TestCase):
    def run_sh(self, script):
        with tempfile.TemporaryDirectory() as cwd:
            return subprocess.run([str(SHELL), "-c", script], cwd=cwd,
                                  env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"},
                                  capture_output=True, text=True, timeout=8)

    def test_native_pipeline(self):
        p = self.run_sh("echo hello world | cat - | wc -c")
        self.assertEqual((p.returncode, p.stdout), (0, "12\n"), p.stderr)

    def test_last_status(self):
        p = self.run_sh("true | false")
        self.assertEqual(p.returncode, 1)

    def test_reject_external(self):
        p = self.run_sh("echo hello | /bin/cat")
        self.assertEqual((p.returncode, p.stdout), (127, ""))

    def test_quoted_words(self):
        p = self.run_sh('echo "hello world" "" | wc -c')
        self.assertEqual((p.returncode, p.stdout), (0, "13\n"), p.stderr)

    def test_variable_words(self):
        p = self.run_sh('X="hello world"; echo "$X" | wc -c')
        self.assertEqual((p.returncode, p.stdout), (0, "12\n"), p.stderr)

    def test_field_splitting(self):
        p = self.run_sh('X="a  b"; echo $X | wc -c')
        self.assertEqual((p.returncode, p.stdout), (0, "4\n"), p.stderr)

    def test_reject_effectful_expansion(self):
        for script in ('echo $(echo bad) | wc -c',
                       'echo ${X:=bad} | wc -c',
                       'echo $((1+2)) | wc -c',
                       'X=bad echo hello | wc -c',
                       'echo *.txt | wc -c'):
            with self.subTest(script=script):
                p = self.run_sh(script)
                self.assertEqual((p.returncode, p.stdout), (125, ""))

    def test_reject_redirect(self):
        p = self.run_sh("echo hello | cat > out")
        self.assertEqual((p.returncode, p.stdout), (125, ""))

    def test_reject_background(self):
        p = self.run_sh("echo hello | cat &")
        self.assertEqual((p.returncode, p.stdout), (125, ""))

if __name__ == "__main__":
    unittest.main()
