"""Exercise unknown USB ID discovery with Windows enumeration fixtures."""

from collections import Counter
from contextlib import redirect_stderr, redirect_stdout
import ctypes
from io import StringIO
import json
import os
import unittest
from unittest.mock import Mock, patch

from exodus import usb_discover
from exodus.mtp_probe import ProbeError, USB_ID


class UsbDiscoverTests(unittest.TestCase):
    def run_discovery(self, *samples):
        clock = 0

        def sleep(seconds):
            nonlocal clock
            clock += seconds

        scan = Mock(side_effect=samples)
        stdout, stderr = StringIO(), StringIO()
        with patch.object(usb_discover.time, "monotonic", lambda: clock), \
                patch.object(usb_discover.time, "sleep", sleep), \
                redirect_stdout(stdout), redirect_stderr(stderr):
            code = usb_discover.main(["--seconds", "5"], scan=scan)
        return code, json.loads(stdout.getvalue()) if stdout.getvalue() else None, stderr.getvalue(), scan

    def test_transient_ids_and_additional_device_with_the_same_id(self):
        baseline = Counter(["1234:0001", "1234:0003"])
        code, report, stderr, _ = self.run_discovery(
            baseline,
            baseline + Counter(["1234:0002"]),
            Counter(["1234:0001"]),
            baseline + Counter(["1234:0001", "1234:0004"]),
            baseline + Counter(["1234:0002", "1234:0004"]),
        )
        self.assertEqual(code, 0)
        self.assertIn("Connect the target", stderr)
        self.assertEqual(report, {
            "schema_version": 1,
            "complete": True,
            "candidates": [
                {"usb_id": "1234:0002", "first_seen_ms": 1000},
                {"usb_id": "1234:0001", "first_seen_ms": 3000},
                {"usb_id": "1234:0004", "first_seen_ms": 3000},
            ],
        })

    def test_no_candidate_after_full_window(self):
        code, report, _, scan = self.run_discovery(*[Counter(["1234:0001"])] * 5)
        self.assertEqual(code, 1)
        self.assertEqual(report, {"schema_version": 1, "complete": True, "candidates": []})
        self.assertEqual(scan.call_count, 5)

    def test_keeps_seen_candidate_when_a_later_sample_fails(self):
        code, report, stderr, scan = self.run_discovery(
            Counter(), Counter(["1234:0002"]), ProbeError("Windows USB enumeration failed."),
        )
        self.assertEqual(code, 2)
        self.assertEqual(report, {
            "schema_version": 1,
            "complete": False,
            "candidates": [{"usb_id": "1234:0002", "first_seen_ms": 1000}],
        })
        self.assertIn("Windows USB enumeration failed", stderr)
        self.assertEqual(scan.call_count, 3)

    def test_baseline_failure_does_not_prompt_or_report_candidates(self):
        code, report, stderr, _ = self.run_discovery(ProbeError("Windows USB enumeration failed."))
        self.assertEqual((code, report), (2, None))
        self.assertEqual(stderr, "USB discovery: Windows USB enumeration failed.\n")

    def test_counts_physical_nodes_and_excludes_interface_and_other_bus_ids(self):
        self.assertEqual(usb_discover.count_usb_ids([
            r"USB\VID_ABCD&PID_1234\private-one",
            r"usb\vid_abcd&pid_1234\private-two",
            r"USB\VID_ABCD&PID_1234&MI_00\private-interface",
            r"PCI\VEN_ABCD&DEV_1234\private-other",
            "",
        ]), Counter({"abcd:1234": 2}))

    @unittest.skipUnless(os.name == "nt", "Requires Windows Configuration Manager")
    def test_enumeration_retries_after_the_device_list_grows(self):
        instance_ids = (
            r"USB\VID_ABCD&PID_1234\private-one" + "\0"
            + r"USB\VID_ABCD&PID_1234\private-two" + "\0\0"
        )

        def set_size(target, enumerator, flags):
            self.assertEqual((enumerator, flags), ("USB", 0x101))
            ctypes.cast(target, ctypes.POINTER(ctypes.c_ulong)).contents.value = len(instance_ids)
            return usb_discover.CR_SUCCESS

        def get_devices(enumerator, buffer, size, flags):
            self.assertEqual((enumerator, size, flags), ("USB", len(instance_ids), 0x101))
            buffer[:] = instance_ids
            return usb_discover.CR_SUCCESS

        config = Mock()
        config.CM_Get_Device_ID_List_SizeW.side_effect = set_size

        def get_ids(*args):
            if config.CM_Get_Device_ID_ListW.call_count == 1:
                return usb_discover.CR_BUFFER_SMALL
            return get_devices(*args)

        config.CM_Get_Device_ID_ListW.side_effect = get_ids
        with patch.object(usb_discover.ctypes, "WinDLL", return_value=config):
            self.assertEqual(usb_discover.read_usb_ids(), Counter({"abcd:1234": 2}))
        self.assertEqual(config.CM_Get_Device_ID_List_SizeW.call_count, 2)

    @unittest.skipUnless(os.name == "nt", "Requires Windows Configuration Manager")
    def test_live_configuration_manager_returns_only_usb_id_counts(self):
        counts = usb_discover.read_usb_ids()
        self.assertIsInstance(counts, Counter)
        self.assertTrue(all(USB_ID.fullmatch(usb_id) and count > 0 for usb_id, count in counts.items()))


if __name__ == "__main__":
    unittest.main()
