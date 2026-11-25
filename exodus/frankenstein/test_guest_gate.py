#!/usr/bin/env python3
"""Regression fixtures for the production first/cold boot evidence gate."""
from pathlib import Path
import subprocess
import tempfile
import unittest

CHECKER = Path(__file__).with_name("verify-guest-evidence.sh")


class GuestGateTests(unittest.TestCase):
    def test_evidence(self):
        for marker, key in (
            ("EXODUS_NATIVE_DBUS_PASS", "EXODUS_GUEST_RESULT"),
            ("EXODUS_RC_COLD_BOOT_PASS", "EXODUS_COLD_GUEST_RESULT"),
        ):
            result = key + "="
            cases = [
                ("valid", [marker, result + "0"], True),
                ("missing marker", [result + "0"], False),
                ("missing result", [marker], False),
                ("nonzero", [marker, result + "1"], False),
                ("zero prefix", [marker, result + "01"], False),
                ("substring marker", ["not " + marker, result + "0"], False),
                ("substring result", [marker, "not " + result + "0"], False),
                ("duplicate", [marker, result + "0", result + "0"], False),
                ("conflict", [marker, result + "0", result + "1"], False),
                ("empty duplicate", [marker, result + "0", result], False),
                ("wrong order", [result + "0", marker], False),
                ("embedded CR", [marker, result + "\r0"], False),
            ]
            for name, lines, accepted in cases + [("CRLF", [marker, result + "0"], True)]:
                with self.subTest(marker=marker, case=name), tempfile.TemporaryDirectory() as tmp:
                    source = Path(tmp) / "serial.log"
                    dest = Path(tmp) / "normalized.log"
                    sep = "\r\n" if name == "CRLF" else "\n"
                    source.write_bytes((sep.join(lines) + sep).encode())
                    run = subprocess.run(["sh", str(CHECKER), str(source), str(dest), marker, key],
                                         capture_output=True, text=True, timeout=5)
                    self.assertEqual(run.returncode == 0, accepted, run.stderr)
                    if accepted:
                        self.assertEqual(run.stdout.splitlines(), [marker, result + "0"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
