import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PROBE = Path(sys.argv.pop(1)).resolve()
PAGE = 4096


class ProbeTests(unittest.TestCase):
    def test_accounting(self):
        dense = bytes(range(256)) * 16
        sparse = b"x" + bytes(PAGE - 1)
        for sample, pages, sparse_count, payload, tail in [
            (b"", 0, 0, 0, 0),
            (b"abc", 0, 0, 3, 3),
            (bytes(PAGE), 1, 1, 8, 0),
            (dense, 1, 0, PAGE, 0),
            (bytes(PAGE) + sparse + dense + b"abc", 3, 2, 8 + 72 + PAGE + 3, 3),
            (sparse * 129 + b"end", 129, 129, 129 * 72 + 3, 3),
        ]:
            with self.subTest(size=len(sample)), tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "input with spaces.bin"
                path.write_bytes(sample)
                result = subprocess.run([PROBE, path], capture_output=True, check=True)
                self.assertEqual(json.loads(result.stdout), {
                    "pages": pages,
                    "sparse_pages": sparse_count,
                    "input_bytes": len(sample),
                    "payload_bytes": payload,
                    "length_index_bytes": 2 * pages,
                    "trailing_bytes": tail,
                })
                self.assertEqual(path.read_bytes(), sample)

    def test_bad_invocations(self):
        with tempfile.TemporaryDirectory() as tmp:
            for args in [[], [Path(tmp) / "missing"]]:
                result = subprocess.run([PROBE, *args], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")

    @unittest.skipUnless(sys.platform.startswith("linux") and Path("/dev/full").exists(),
                         "Requires Linux /dev/full")
    def test_output_flush_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sample.bin"
            sample = bytes(PAGE) + b"tail"
            path.write_bytes(sample)
            with open("/dev/full", "wb") as output:
                result = subprocess.run([PROBE, path], stdout=output, stderr=subprocess.PIPE)
            self.assertEqual(path.read_bytes(), sample)
            self.assertGreater(result.returncode, 0)
            self.assertIn(b"Could not write probe results", result.stderr)


if __name__ == "__main__":
    unittest.main()
