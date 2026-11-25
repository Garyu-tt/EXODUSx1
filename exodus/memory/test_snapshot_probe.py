import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PROBE = Path(sys.argv.pop(1)).resolve()
PAGE = 4096


class SnapshotProbeTests(unittest.TestCase):
    def probe(self, reference, current):
        with tempfile.TemporaryDirectory() as scratch:
            base = Path(scratch) / "reference α.bin"
            sample = Path(scratch) / "current β.bin"
            base.write_bytes(reference)
            sample.write_bytes(current)
            result = subprocess.run([PROBE, base, sample], capture_output=True)
            self.assertEqual(base.read_bytes(), reference)
            self.assertEqual(sample.read_bytes(), current)
            return result

    def test_mixed_pages_include_reference_cost_and_raw_tail(self):
        ramp = bytes(range(256)) * 16
        changed = bytearray(ramp)
        changed[137] ^= 0x80
        result = self.probe(ramp * 2 + b"old", changed + bytes(PAGE) + b"new")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {
            "pages": 2, "relative_pages": 1, "trailing_bytes": 3,
            "standalone_frame_bytes": 4097 + 9 + 3,
            "selected_frame_bytes": 56 + 9 + 3,
            "retained_reference_bytes": PAGE,
            "selected_plus_reference_bytes": 56 + 9 + 3 + PAGE,
        })

    def test_empty_and_partial_samples_require_no_reference(self):
        for sample in (b"", b"abc"):
            with self.subTest(sample=sample):
                result = self.probe(sample, sample)
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report["retained_reference_bytes"], 0)
                self.assertEqual(report["pages"], 0)
                self.assertEqual(report["trailing_bytes"], len(sample))
                self.assertEqual(report["selected_plus_reference_bytes"], len(sample))

    def test_mismatched_lengths_fail_without_partial_report(self):
        for reference, current in ((bytes(PAGE), bytes(PAGE + 1)),
                                   (bytes(PAGE * 2), bytes(PAGE))):
            with self.subTest(reference=len(reference), current=len(current)):
                result = self.probe(reference, current)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"equal byte lengths", result.stderr)
                self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    unittest.main()
