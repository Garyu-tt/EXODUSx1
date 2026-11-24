import os
from pathlib import Path
import subprocess
import tempfile
import unittest

class TerminalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.bin = Path(cls.build.name) / "terminal"
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-Wall", "-Wextra",
                        "-Werror", str(Path(__file__).with_name("main.c")),
                        "-o", str(cls.bin)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def run_commands(self, commands, cwd):
        return subprocess.run([str(self.bin)], input=commands, text=True,
                              cwd=cwd, capture_output=True, timeout=5, check=True).stdout

    def test_navigation(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            (p / "folder space").mkdir()
            (p / "folder space" / "sample.txt").write_text("hello")
            result = self.run_commands("ls\ncd folder space\npwd\ncat sample.txt\nexit\necho ignored\n", d)
            self.assertIn("folder space", result)
            self.assertIn("hello", result)
            self.assertNotIn("ignored", result)

    def test_safe_output(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "data").write_bytes(b"\x1b[2J\x00" + b"a" * 5000)
            result = self.run_commands("cat data\ncat .\n", d)
            self.assertIn(r"\x1b[2J\x00", result)
            self.assertNotIn("\x1b", result)
            self.assertIn("[output limited]", result)
            self.assertIn("regular files only", result)

    def test_long_line_and_unknown(self):
        with tempfile.TemporaryDirectory() as d:
            result = self.run_commands("echo " + "x"*700 + "\necho ok\nunknown\nstatus\n", d)
            self.assertIn("line too long", result)
            self.assertIn("ok", result)
            self.assertIn("unknown command", result)
            self.assertIn("host reference", result)
            self.assertNotIn("x"*100, result)

if __name__ == "__main__":
    unittest.main()
