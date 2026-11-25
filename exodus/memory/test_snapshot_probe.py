import json
import os
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
            result = subprocess.run([PROBE, base, sample], capture_output=True, timeout=5)
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

    def test_rejects_devices_and_directories_in_either_position(self):
        with tempfile.TemporaryDirectory() as scratch:
            sample = Path(scratch) / "empty.bin"
            sample.write_bytes(b"")
            for invalid in (os.devnull, scratch):
                for position in (0, 1):
                    with self.subTest(invalid=invalid, position=position):
                        inputs = [sample, sample]
                        inputs[position] = invalid
                        result = subprocess.run([PROBE, *inputs], capture_output=True, timeout=5)
                        self.assertGreater(result.returncode, 0)
                        self.assertEqual(result.stdout, b"")
                        self.assertIn(b"regular snapshot file", result.stderr)
                        label = b"Reference snapshot" if position == 0 else b"Current snapshot"
                        self.assertIn(label, result.stderr)
            self.assertEqual(sample.read_bytes(), b"")

    @unittest.skipUnless(hasattr(os, "mkfifo"), "Requires a POSIX FIFO")
    def test_rejects_writerless_fifo_in_either_position(self):
        with tempfile.TemporaryDirectory() as scratch:
            sample = Path(scratch) / "empty.bin"
            sample.write_bytes(b"")
            fifo = Path(scratch) / "snapshot.fifo"
            os.mkfifo(fifo)
            for position in (0, 1):
                with self.subTest(position=position):
                    inputs = [sample, sample]
                    inputs[position] = fifo
                    result = subprocess.run([PROBE, *inputs], capture_output=True, timeout=5)
                    self.assertGreater(result.returncode, 0)
                    self.assertEqual(result.stdout, b"")
                    self.assertIn(b"regular snapshot file", result.stderr)
            self.assertEqual(sample.read_bytes(), b"")

    def test_same_unicode_regular_file_can_supply_both_snapshots(self):
        with tempfile.TemporaryDirectory() as scratch:
            sample = Path(scratch) / "測試" / "snapshot 🧪.bin"
            sample.parent.mkdir()
            content = bytes(range(256)) * 16 + b"end"
            sample.write_bytes(content)
            result = subprocess.run([PROBE, sample, sample], capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout), {
                "pages": 1, "relative_pages": 1, "trailing_bytes": 3,
                "standalone_frame_bytes": 4097 + 3,
                "selected_frame_bytes": 13 + 3,
                "retained_reference_bytes": PAGE,
                "selected_plus_reference_bytes": 13 + 3 + PAGE,
            })
            self.assertEqual(sample.read_bytes(), content)

    def test_accepts_regular_file_links_in_either_position(self):
        with tempfile.TemporaryDirectory() as scratch:
            base = Path(scratch) / "reference.bin"
            current = Path(scratch) / "current.bin"
            reference = bytes(range(256)) * 16
            changed = bytearray(reference)
            changed[137] ^= 0x80
            base.write_bytes(reference)
            current.write_bytes(changed)
            links = [Path(scratch) / "reference-link.bin", Path(scratch) / "current-link.bin"]
            try:
                links[0].symlink_to(base.name)
                links[1].symlink_to(current.name)
            except (OSError, NotImplementedError) as exc:
                self.skipTest(f"creating file symlinks unavailable: {exc}")
            baseline = subprocess.run([PROBE, base, current], capture_output=True, timeout=5)
            self.assertEqual(baseline.returncode, 0, baseline.stderr)
            for position in (0, 1):
                with self.subTest(position=position):
                    inputs = [base, current]
                    inputs[position] = links[position]
                    result = subprocess.run([PROBE, *inputs], capture_output=True, timeout=5)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout, baseline.stdout)
            self.assertEqual(base.read_bytes(), reference)
            self.assertEqual(current.read_bytes(), changed)


if __name__ == "__main__":
    unittest.main()
