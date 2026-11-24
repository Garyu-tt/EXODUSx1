from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


PROBE = Path(sys.argv.pop(1)).resolve()


def nro_image():
    image = bytearray(0x3000)
    image[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", image, 0x18, len(image))
    struct.pack_into("<6I", image, 0x20, 0, 0x1000, 0x1000, 0x1000, 0x2000, 0x1000)
    return image


class AutorunProbeTests(unittest.TestCase):
    def probe(self, path):
        result = subprocess.run([PROBE, path], check=True, capture_output=True, text=True)
        return result.stdout.strip()

    def test_missing_and_nonregular_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(self.probe(root / "missing.nro"), "missing")
            self.assertEqual(self.probe(root), "unreadable")

    def test_image_with_optional_asset_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "runtime file.nro"
            for image in (nro_image(), nro_image() + b"ASET" + bytes(52)):
                with self.subTest(size=len(image)):
                    path.write_bytes(image)
                    self.assertEqual(self.probe(path), "ready")
                    self.assertEqual(path.read_bytes(), image)

    def test_truncated_and_unrecognized_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "runtime.nro"
            for data in (b"", nro_image()[:0x7f], nro_image()[:-1], bytes(0x3000)):
                with self.subTest(size=len(data)):
                    path.write_bytes(data)
                    self.assertEqual(self.probe(path), "invalid-nro")

    def test_inconsistent_segment_ranges(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "runtime.nro"
            for offset, field in (
                (0x18, 0x1000),     # Image ends before the declared segments.
                (0x20, 0x1000),     # Text must start with the header.
                (0x24, 0),          # Text must include the header.
                (0x28, 0),          # Read-only segment overlaps text.
                (0x2c, 0x0400),     # Segment sizes must be page-aligned.
                (0x30, 0x4000),     # Writable segment starts outside the image.
                (0x34, 0xfffff000), # Segment size must fit without overflow.
            ):
                with self.subTest(offset=offset, field=field):
                    image = nro_image()
                    struct.pack_into("<I", image, offset, field)
                    path.write_bytes(image)
                    self.assertEqual(self.probe(path), "invalid-nro")


if __name__ == "__main__":
    unittest.main()
