"""Exercise USB diagnosis and reconnect reporting with Windows PnP fixtures."""

from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
import unittest
from unittest.mock import Mock, patch

from exodus import mtp_probe


USB_ID = "1234:abcd"
PARENT = r"USB\VID_1234&PID_ABCD\private-device-instance-A"
PORTABLE = r"USB\VID_1234&PID_ABCD&MI_01\private-interface-instance"
MTP_ID = r"USB\MS_COMP_MTP&MS_SUBCOMP_01"


def node(instance_id, device_class="USB", problem=0, compatible=None, parent=None):
    return {
        "PNPDeviceID": instance_id,
        "PNPClass": device_class,
        "ConfigManagerErrorCode": problem,
        "CompatibleID": compatible,
        "ParentInstanceId": parent,
    }


def snapshots(*device_lists):
    return [mtp_probe.classify(devices, USB_ID) for devices in device_lists]


def timed_samples(*responses):
    pending = iter(responses)

    def sample(usb_id, timeout=None):
        delay, snapshot = next(pending)
        if delay:
            mtp_probe.time.sleep(delay)
        return snapshot

    return sample


class MtpProbeTests(unittest.TestCase):
    def setUp(self):
        self.ready = [node(PARENT), node(PORTABLE, "WPD", compatible=[MTP_ID], parent=PARENT.lower())]

    def run_probe(self, sample, *args):
        clock = 0

        def sleep(seconds):
            nonlocal clock
            clock += seconds

        stdout, stderr = StringIO(), StringIO()
        with patch.object(mtp_probe.time, "monotonic", lambda: clock), \
                patch.object(mtp_probe.time, "sleep", sleep), \
                redirect_stdout(stdout), redirect_stderr(stderr):
            code = mtp_probe.main(["--usb-id", USB_ID.upper(), *args], sample=sample)
        output = stdout.getvalue()
        self.assertNotIn("private-", output + stderr.getvalue())
        return code, json.loads(output) if output else None, stderr.getvalue()

    def test_ignores_other_products_and_non_usb_nodes(self):
        sample = Mock(return_value=snapshots([
            node(r"USB\VID_1234&PID_ABCE\other", "WPD", compatible=[MTP_ID]),
            node(r"SWD\WPDBUSENUM\VID_1234&PID_ABCD", "WPD"),
        ])[0])
        code, report, errors = self.run_probe(sample)
        self.assertEqual((code, errors), (1, ""))
        self.assertEqual(report, {
            "schema_version": 1,
            "usb_id": USB_ID,
            "initial": {
                "state": "not_present", "usb_nodes": 0, "wpd_nodes": 0,
                "problem_codes": [], "mtp_advertised": False,
            },
            "reconnect": None,
        })

    def test_reports_healthy_wpd_and_mtp_compatible_id(self):
        sample = Mock(return_value=snapshots(self.ready)[0])
        code, report, errors = self.run_probe(sample)
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual(report["initial"], {
            "state": "wpd_ready", "usb_nodes": 2, "wpd_nodes": 1,
            "problem_codes": [], "mtp_advertised": True,
        })
        sample.assert_called_once_with(USB_ID)

    def test_wpd_alone_does_not_mean_the_device_advertises_mtp(self):
        [snapshot] = snapshots([node(PARENT.lower(), "wpd", compatible=[r"USB\CLASS_06"])])
        self.assertEqual(snapshot.state, "wpd_ready")
        self.assertFalse(snapshot.mtp_advertised)

    def test_reports_usb_only_until_matching_driver_and_parent_are_ready(self):
        for devices in ([node(PARENT)], self.ready[1:], [node(PARENT, "WPD", problem=None)]):
            with self.subTest(devices=devices):
                self.assertEqual(snapshots(devices)[0].state, "usb_only")

    def test_healthy_other_wpd_interface_does_not_mask_mtp_driver_failure(self):
        [snapshot] = snapshots([
            node(PARENT),
            node(PORTABLE, None, problem=28, compatible=[MTP_ID], parent=PARENT),
            node(PORTABLE.replace("MI_01", "MI_02"), "WPD", parent=PARENT),
        ])
        self.assertEqual((snapshot.state, snapshot.problem_codes), ("driver_problem", (28,)))

    def test_unlinked_interface_cannot_supply_wpd_readiness(self):
        for stale_parent in (None, PARENT + "2"):
            with self.subTest(stale_parent=stale_parent):
                [snapshot] = snapshots([
                    node(PARENT),
                    node(PORTABLE, "WPD", compatible=[MTP_ID], parent=stale_parent),
                ])
                self.assertEqual(
                    (snapshot.state, snapshot.usb_nodes, snapshot.wpd_nodes, snapshot.mtp_advertised),
                    ("usb_only", 2, 0, False),
                )

    def test_unrelated_mtp_driver_error_leaves_current_wpd_ready(self):
        [snapshot] = snapshots([
            node(PARENT),
            node(PORTABLE, None, problem=28, compatible=[MTP_ID], parent=PARENT + "2"),
            node(PORTABLE.replace("MI_01", "MI_02"), "WPD", parent=PARENT),
        ])
        self.assertEqual(
            (snapshot.state, snapshot.problem_codes, snapshot.mtp_advertised),
            ("wpd_ready", (), False),
        )

    def test_ambiguous_usb_id_never_reports_wpd_ready(self):
        [snapshot] = snapshots(self.ready + [node(PARENT + "2")])
        self.assertEqual(snapshot.state, "ambiguous")

    def test_reconnect_recovers_same_device_after_observed_disconnect(self):
        sample = Mock(side_effect=snapshots(self.ready, [node(PARENT)], [], self.ready))
        code, report, errors = self.run_probe(sample, "--reconnect", "6")
        self.assertEqual(code, 0)
        self.assertIn("reconnect to the same port", errors)
        self.assertEqual(report["reconnect"]["state"], "reconnected")
        self.assertTrue(report["reconnect"]["disconnect_seen"])
        self.assertEqual(report["reconnect"]["last"]["state"], "wpd_ready")
        self.assertEqual(sample.call_count, 4)

    def test_timed_out_when_usb_remains_present(self):
        sample = Mock(return_value=snapshots(self.ready)[0])
        code, report, _ = self.run_probe(sample, "--reconnect", "5")
        self.assertEqual(code, 1)
        self.assertEqual(report["reconnect"]["state"], "timed_out")
        self.assertFalse(report["reconnect"]["disconnect_seen"])
        self.assertEqual(sample.call_count, 5)

    def test_reconnect_rejects_different_instance_with_the_same_usb_id(self):
        substitute = [node(PARENT + "2"), node(PORTABLE, "WPD", compatible=[MTP_ID], parent=PARENT + "2")]
        sample = Mock(side_effect=snapshots(self.ready, [], substitute))
        code, report, _ = self.run_probe(sample, "--reconnect", "5")
        self.assertEqual(code, 1)
        self.assertEqual(report["reconnect"]["state"], "target_changed")
        self.assertTrue(report["reconnect"]["disconnect_seen"])

    def test_reconnect_requires_recovery_of_previously_advertised_mtp(self):
        wpd_only = [node(PARENT), node(PORTABLE, "WPD", parent=PARENT)]
        sample = Mock(side_effect=snapshots(self.ready, [], wpd_only, wpd_only, wpd_only))
        code, report, _ = self.run_probe(sample, "--reconnect", "5")
        self.assertEqual(code, 1)
        self.assertEqual(report["reconnect"]["state"], "timed_out")
        self.assertTrue(report["reconnect"]["disconnect_seen"])

    def test_reconnect_cannot_use_stale_wpd_for_the_original_parent(self):
        stale_wpd = [node(PARENT), node(PORTABLE, "WPD", compatible=[MTP_ID], parent=PARENT + "2")]
        sample = Mock(side_effect=snapshots(self.ready, [], stale_wpd, stale_wpd, stale_wpd))
        code, report, _ = self.run_probe(sample, "--reconnect", "5")
        self.assertEqual(code, 1)
        self.assertEqual(report["reconnect"]["state"], "timed_out")
        self.assertTrue(report["reconnect"]["disconnect_seen"])

    def test_reconnect_does_not_wait_for_an_unready_baseline(self):
        sample = Mock(return_value=snapshots([node(PARENT)])[0])
        code, report, errors = self.run_probe(sample, "--reconnect", "30")
        self.assertEqual((code, errors), (1, ""))
        self.assertEqual(report["reconnect"]["state"], "baseline_not_ready")
        sample.assert_called_once_with(USB_ID)

    def test_invalid_windows_data_stays_distinct_from_absent_device(self):
        def malformed_sample(usb_id):
            return mtp_probe.classify([node(PARENT, problem="unknown")], usb_id)

        code, report, errors = self.run_probe(malformed_sample)
        self.assertEqual((code, report), (2, None))
        self.assertIn("invalid USB driver data", errors)

    def test_query_timeout_in_reconnect_uses_the_remaining_window(self):
        sample = Mock(side_effect=[snapshots(self.ready)[0], mtp_probe.ProbeTimeout("timed out")])
        code, report, _ = self.run_probe(sample, "--reconnect", "5")
        self.assertEqual(code, 1)
        self.assertEqual(report["reconnect"]["state"], "timed_out")
        sample.assert_called_with(USB_ID, timeout=4)

    def test_reconnect_rejects_readiness_at_or_after_the_deadline(self):
        ready, absent = snapshots(self.ready, [])
        for delay in (3, 3.25):
            with self.subTest(delay=delay):
                sample = timed_samples((0, ready), (0, absent), (delay, ready))
                code, report, _ = self.run_probe(sample, "--reconnect", "5")
                self.assertEqual(code, 1)
                self.assertEqual(report["reconnect"], {
                    "state": "timed_out", "disconnect_seen": True, "last": absent.report(),
                })


    def test_observe_cross_port_from_absent(self):
        other = [node(PARENT + "2"), node(PORTABLE, "WPD", compatible=[MTP_ID], parent=PARENT + "2")]
        sample = Mock(side_effect=snapshots([], [node(PARENT)], self.ready, [], other))
        code, report, _ = self.run_probe(sample, "--observe", "5")
        self.assertEqual(code, 0)
        events = report["observation"]["events"]
        self.assertEqual([x["state"] for x in events],
                         ["not_present", "usb_only", "wpd_ready", "not_present", "wpd_ready"])
        self.assertTrue(events[-1]["parent_changed"])
        self.assertEqual([x["elapsed_ms"] for x in events], [0, 1000, 2000, 3000, 4000])

    def test_observe_never_claims_transfer(self):
        sample = Mock(return_value=snapshots([])[0])
        code, report, _ = self.run_probe(sample, "--observe", "5")
        self.assertEqual(code, 0)
        self.assertEqual(len(report["observation"]["events"]), 1)
        self.assertEqual(report["initial"]["state"], "not_present")

    def test_observe_timeout_is_incomplete(self):
        sample = Mock(side_effect=[snapshots([])[0], mtp_probe.ProbeTimeout("timed out")])
        code, report, _ = self.run_probe(sample, "--observe", "5")
        self.assertEqual(code, 1)
        self.assertFalse(report["observation"]["complete"])
        self.assertEqual(report["observation"]["reason"], "query_timeout")

    def test_observe_discards_a_sample_completed_at_or_after_the_deadline(self):
        absent, ready = snapshots([], self.ready)
        for delay in (3, 3.25):
            with self.subTest(delay=delay):
                sample = timed_samples((0, absent), (0, ready), (delay, absent))
                code, report, _ = self.run_probe(sample, "--observe", "5")
                self.assertEqual(code, 1)
                observation = report["observation"]
                self.assertFalse(observation["complete"])
                self.assertEqual(observation["reason"], "query_timeout")
                self.assertEqual([(e["elapsed_ms"], e["state"]) for e in observation["events"]],
                                 [(0, "not_present"), (1000, "wpd_ready")])

    def test_observe_keeps_transitions_when_enumeration_fails_mid_window(self):
        for error, reason in ((mtp_probe.ProbeTimeout("timed out"), "query_timeout"),
                              (mtp_probe.ProbeError("enumeration failed"), "query_failed")):
            with self.subTest(reason=reason):
                sample = Mock(side_effect=[*snapshots([], self.ready, []), error])
                code, report, errors = self.run_probe(sample, "--observe", "120")
                self.assertEqual(code, 2)
                self.assertIn(str(error), errors)
                self.assertEqual(report["initial"]["state"], "not_present")
                observation = report["observation"]
                self.assertFalse(observation["complete"])
                self.assertEqual(observation["reason"], reason)
                self.assertEqual([(e["elapsed_ms"], e["state"]) for e in observation["events"]],
                                 [(0, "not_present"), (1000, "wpd_ready"), (2000, "not_present")])

    def test_observe_requires_a_successful_baseline(self):
        sample = Mock(side_effect=mtp_probe.ProbeError("enumeration failed"))
        code, report, errors = self.run_probe(sample, "--observe", "120")
        self.assertEqual(code, 2)
        self.assertIsNone(report)
        self.assertIn("enumeration failed", errors)


if __name__ == "__main__":
    unittest.main()
