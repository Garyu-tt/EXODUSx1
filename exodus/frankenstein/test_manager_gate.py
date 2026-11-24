#!/usr/bin/env python3
"""Exercise the production POSIX manager evidence checker without a VM."""
from pathlib import Path
import subprocess
import tempfile
import unittest

CHECKER = Path(__file__).with_name("verify-manager-evidence.sh")
PROTOCOL = "EXODUS_MANAGER_PROTOCOL_BUILD_PASS"
RESULT = "EXODUS_MANAGER_DAEMON_CHECK_RESULT="
PASS = "EXODUS_MANAGER_DAEMON_COMPILE_PASS"


class ManagerGateTests(unittest.TestCase):
    def check(self, lines, accepted, newline="\n"):
        with tempfile.TemporaryDirectory() as tmp:
            serial = Path(tmp) / "serial.log"
            evidence = Path(tmp) / "manager-evidence.txt"
            serial.write_bytes((newline.join(lines) + newline).encode())
            run = subprocess.run(
                ["sh", str(CHECKER), str(serial), str(evidence)],
                capture_output=True, text=True, timeout=5,
            )
            self.assertEqual(evidence.read_bytes(), serial.read_bytes().replace(b"\r", b""))
            self.assertEqual(run.returncode == 0, accepted, run.stderr)
            self.assertEqual(PASS in run.stdout.splitlines(), accepted)

    def test_result_records(self):
        cases = [
            ("success", [PROTOCOL, RESULT + "0"], True),
            ("compiler failure", [PROTOCOL, RESULT + "101"], False),
            ("missing result", [PROTOCOL], False),
            ("missing protocol", [RESULT + "0"], False),
            ("duplicate success", [PROTOCOL, RESULT + "0", RESULT + "0"], False),
            ("conflict", [PROTOCOL, RESULT + "0", RESULT + "101"], False),
            ("prefix", [PROTOCOL, "log " + RESULT + "0"], False),
            ("empty after", [PROTOCOL, RESULT + "0", RESULT], False),
            ("empty before", [PROTOCOL, RESULT, RESULT + "0"], False),
            ("empty", [PROTOCOL, RESULT], False),
            ("whitespace", [PROTOCOL, RESULT + " "], False),
            ("zero padded", [PROTOCOL, RESULT + "00"], False),
            ("suffix", [PROTOCOL, RESULT + "0x"], False),
            ("protocol substring", ["log " + PROTOCOL, RESULT + "0"], False),
        ]
        for name, lines, accepted in cases:
            with self.subTest(name=name):
                self.check(lines, accepted)
        with self.subTest(name="CRLF"):
            self.check([PROTOCOL, RESULT + "0"], True, "\r\n")


if __name__ == "__main__":
    unittest.main(verbosity=2)
