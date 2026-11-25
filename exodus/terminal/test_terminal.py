import os
from pathlib import Path
import subprocess
import struct
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

    def test_diagnostics(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "sample").write_bytes(b"abc")
            result = self.run_commands("stat sample\nstat .\nstat missing\nid\nmem\n", d)
            self.assertIn("type=file size_bytes=3", result)
            self.assertIn("type=directory", result)
            self.assertIn("stat: error", result)
            self.assertRegex(result, r"process_id=[1-9][0-9]*")
            self.assertIn("process_memory_limit_bytes=unknown", result)
            self.assertIn("not system RAM or GPU VRAM", result)

    def test_nro_and_handoff(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            image = bytearray(12288)
            image[16:20] = b"NRO0"
            struct.pack_into("<I", image, 24, len(image))
            struct.pack_into("<IIIIII", image, 32, 0, 4096, 4096, 4096, 8192, 4096)
            (p / "valid.nro").write_bytes(image)
            (p / "short.nro").write_bytes(image[:128])
            struct.pack_into("<I", image, 40, 0)
            (p / "overlap.nro").write_bytes(image)
            result = self.run_commands("nro valid.nro\nnro short.nro\nnro overlap.nro\nnro missing\napps\nrun ssh\nrun autorun\nrun unknown\nps\nsvc\necho still-running\n", d)
            self.assertEqual(result.count("NRO structural preflight passed"), 1)
            self.assertEqual(result.count("NRO missing or invalid"), 3)
            self.assertIn("native loader unavailable", result)
            self.assertIn("run: native loader unavailable (host reference); "
                          "target=sdmc:/switch/exodus-ssh/exodus-ssh.nro",
                          result.splitlines())
            self.assertIn("run: native loader unavailable (host reference); "
                          "target=sdmc:/switch/wine/wine-nx-runtime.nro",
                          result.splitlines())
            self.assertIn("unknown app", result)
            self.assertIn("Current process only", result)
            self.assertIn("service state unknown", result)
            self.assertIn("still-running", result)

    def test_freebsd_path_atoms(self):
        with tempfile.TemporaryDirectory() as d:
            result = self.run_commands("basename /usr/bin/sh\ndirname /usr/bin/sh\nbasename ////\ndirname ////\nbasename file\ndirname file\nbasename /a/b///\ndirname /a/b///\nbasename\ndirname\n", d)
            self.assertEqual(result.splitlines(), ["sh", "/usr/bin", "/", "/", "file", ".", "b", "/a", ".", "."])

    def test_abi_smoke(self):
        with tempfile.TemporaryDirectory() as d:
            result = self.run_commands("abi\n", d)
            self.assertIn("PASS host process identity", result)
            self.assertIn("PASS monotonic clock", result)
            self.assertIn("PASS cwd stat", result)
            self.assertIn("PASS directory read", result)
            self.assertIn("ABI smoke: 4 passed, 0 failed", result)
            self.assertIn("process/pipe/signal semantics not tested", result)
            self.assertEqual(list(Path(d).iterdir()), [])

    def test_long_line_and_unknown(self):
        with tempfile.TemporaryDirectory() as d:
            result = self.run_commands("echo " + "x"*700 + "\necho ok\nunknown\nstatus\n", d)
            self.assertIn("line too long", result)
            self.assertIn("ok", result)
            self.assertIn("unknown command", result)
            self.assertIn("host reference", result)
            self.assertNotIn("x"*100, result)


class ToolReturnTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        cls.bin = Path(cls.build.name) / "return-marker"
        source = Path(cls.build.name) / "return-marker.c"
        source.write_text('''#include <stdio.h>
#include "tool_return.h"
int main(int argc, char **argv) {
    char *missing_marker[] = {"terminal", NULL};
    if (exodus_return_requested(0, NULL) ||
        exodus_return_requested(2, NULL) ||
        exodus_return_requested(2, missing_marker)) return 1;
    printf("%d\\n", exodus_return_requested(argc, argv));
    return 0;
}
''')
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-Wall", "-Wextra",
                        "-Werror", "-I", str(Path(__file__).parent), str(source),
                        "-o", str(cls.bin)], check=True)

    def requested(self, *args):
        return subprocess.run([str(self.bin), *args], text=True,
                              capture_output=True, timeout=5, check=True).stdout.strip()

    def test_exact_opt_in(self):
        self.assertEqual(self.requested("--exodus-return-home"), "1")

    def test_missing_or_inexact_marker(self):
        for args in [(), ("",), ("--exodus-return",), ("--exodus-return-home=1",),
                     ("--exodus-return-home ",), ("sdmc:/switch/other.nro",)]:
            with self.subTest(args=args):
                self.assertEqual(self.requested(*args), "0")

    def test_marker_must_be_only_argument(self):
        for args in [("--exodus-return-home", "sdmc:/switch/other.nro"),
                     ("sdmc:/switch/other.nro", "--exodus-return-home"),
                     ("--exodus-return-home", "--exodus-return-home")]:
            with self.subTest(args=args):
                self.assertEqual(self.requested(*args), "0")


if __name__ == "__main__":
    unittest.main()
