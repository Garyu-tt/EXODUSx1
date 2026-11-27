import hashlib
import unittest
from tumor_board_report import inspect_report

# Synthetic text matching the photographed R02 field layout, not an
# acquired SD artifact and not proof of physical execution.
SAMPLE = b"""schema=1
kind=bpmp_board_inventory
probe_revision=2
hidrev=0x00012127
chip_id=33
chip_major=2
chip_minor=1
hw_type=2
dram_id=14
dram_mib=4096
adsp_execution=not_tested
a57_el2=not_tested
dual_kernel=not_tested
deployment_authorized=0
"""


class BoardReport(unittest.TestCase):
    def test_inventory_only(self):
        for raw in (SAMPLE, SAMPLE.replace(b"\n", b"\r\n")):
            with self.subTest(raw=raw):
                r = inspect_report(raw)
                self.assertEqual(r["soc"], "mariko")
                self.assertEqual(r["board_hint"], "hoag")
                self.assertEqual(r["observed"]["dram_mib"], 4096)
                self.assertEqual(r["sha256"], hashlib.sha256(raw).hexdigest())
                self.assertFalse(r["physical_provenance_verified"])
                self.assertEqual(r["physical_gates_passed"], [])
                self.assertFalse(r["deployment_authorized"])

    def test_reject_bad_records(self):
        for raw in (
            b"", b"x" * 1025, SAMPLE + b"chip_id=33\n",
            SAMPLE + b"extra=1\n", SAMPLE.replace(b"chip_id=33", b"chip_id=%u"),
            SAMPLE.replace(b"chip_minor=1", b"chip_minor=2"),
            SAMPLE.replace(b"dram_mib=4096", b"dram_mib=0"),
            SAMPLE.replace(b"dram_mib=4096", b"dram_mib=65536"),
            SAMPLE.replace(b"probe_revision=2\n", b""),
            SAMPLE.replace(b"deployment_authorized=0", b"deployment_authorized=1"),
            SAMPLE.replace(b"a57_el2=not_tested", b"a57_el2=pass"),
            SAMPLE.replace(b"adsp_execution=not_tested", b"adsp_execution=pass"),
            SAMPLE.replace(b"dual_kernel=not_tested", b"dual_kernel=pass"),
            SAMPLE.replace(b"hw_type=2", b"hw_type=\xff"),
        ):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                inspect_report(raw)


if __name__ == "__main__":
    unittest.main()
