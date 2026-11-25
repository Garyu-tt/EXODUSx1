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
        sparse = bytes(range(1, 65)) + bytes(PAGE - 64)
        pixel_run = bytes.fromhex("44 77 aa ff") * (PAGE // 4)
        single_byte = b"x" + bytes(PAGE - 1)
        for sample, pages, sparse_count, word_run_count, payload, tail in [
            (b"", 0, 0, 0, 0, 0),
            (b"abc", 0, 0, 0, 3, 3),
            (bytes(PAGE), 1, 1, 0, 8, 0),
            (dense, 1, 0, 0, PAGE, 0),
            (single_byte, 1, 0, 1, 47, 0),
            (bytes(PAGE) + sparse + dense + pixel_run + b"abc", 4, 2, 1, 8 + 72 + PAGE + 41 + 3, 3),
            (sparse * 128 + pixel_run + b"end", 129, 128, 1, 128 * 72 + 41 + 3, 3),
        ]:
            with self.subTest(size=len(sample)), tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "input with spaces.bin"
                path.write_bytes(sample)
                result = subprocess.run([PROBE, path], capture_output=True, check=True)
                self.assertEqual(json.loads(result.stdout), {
                    "pages": pages,
                    "sparse_pages": sparse_count,
                    "word_run_pages": word_run_count,
                    "input_bytes": len(sample),
                    "payload_bytes": payload,
                    "length_index_bytes": 2 * pages,
                    "trailing_bytes": tail,
                })
                self.assertEqual(path.read_bytes(), sample)

    def test_unicode_sample_path(self):
        sample = bytes(PAGE) + b"end"
        with tempfile.TemporaryDirectory() as tmp:
            plain_path = Path(tmp) / "sample.bin"
            plain_path.write_bytes(sample)
            baseline = subprocess.run([PROBE, plain_path], capture_output=True, check=True)

            unicode_path = Path(tmp) / "測試" / "sample 🧪.bin"
            unicode_path.parent.mkdir()
            plain_path.rename(unicode_path)
            result = subprocess.run([PROBE, unicode_path], capture_output=True)

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout), json.loads(baseline.stdout))
            self.assertEqual(unicode_path.read_bytes(), sample)

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
