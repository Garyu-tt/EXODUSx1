import os
import signal
import sys
import time
import unittest
from dataclasses import replace
from unittest.mock import patch

from exodus.task_center import TaskCenter, UNKNOWN
from exodus.task_center_child import ManagedChild


@unittest.skipUnless(hasattr(os, "posix_spawn") and hasattr(os, "WCONTINUED"),
                     "POSIX child observation required")
class ManagedChildTests(unittest.TestCase):
    def setUp(self):
        self.child = ManagedChild(1, "test-child", 1,
                                  [sys.executable, "-c", "import time; time.sleep(60)"])
        self.addCleanup(self.cleanup)
        self.center = TaskCenter()
        self.center.publish(*self.child.snapshot())
        self.center.reconcile(None)  # test-only positively released bundle

    def cleanup(self):
        if self.child.poll() not in ("exited", UNKNOWN):
            # Test fixture only. Production adapter has no forced kill.
            try:
                os.kill(self.child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(self.child.pid, 0)
            self.child.terminal = True

    def operate(self, operation, expected):
        self.center.publish(*self.child.snapshot())
        p = self.center.request_lifecycle(self.child.key, operation)
        self.assertTrue(self.child.dispatch(p))
        self.assertFalse(self.child.dispatch(p))
        self.center.acknowledge_lifecycle(p.token, p.target, True)
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            self.center.publish(*self.child.snapshot())
            if self.center.lifecycle_result:
                break
            time.sleep(0.01)
        self.assertEqual(self.center.lifecycle_result, (p.token, "confirmed"))
        self.assertEqual(self.child.state, expected)

    def test_real_child_stop_continue_term(self):
        self.operate("suspend", "suspended")
        self.operate("resume", "running")
        self.operate("terminate", "exited")
        self.assertTrue(self.child.terminal)
        self.assertEqual(self.child.capabilities(), frozenset())

    def test_wrong_identity_and_stale_sequence_no_signal(self):
        p = self.center.request_lifecycle(self.child.key, "suspend")
        with patch("exodus.task_center_child.os.kill") as kill:
            self.assertFalse(self.child.dispatch(replace(p, target=("freebsd", 1, "other", 1))))
            self.assertFalse(self.child.dispatch(replace(p, sequence=99)))
            self.assertFalse(self.child.dispatch(replace(p, deadline=0)))
            kill.assert_not_called()

    def test_malformed_request_never_signals_or_consumes_token(self):
        p = self.center.request_lifecycle(self.child.key, "suspend")
        bad = [
            replace(p, target=("freebsd", True, "test-child", 1)),
            replace(p, target=("freebsd", 1, "test-child", True)),
            replace(p, sequence=True), replace(p, deadline=float("nan")),
            replace(p, deadline=float("inf")), replace(p, deadline=10**1000),
            replace(p, deadline="later"), replace(p, accepted=0),
            replace(p, operation=[]),
        ]
        with patch("exodus.task_center_child.os.kill") as kill:
            for request in bad:
                with self.subTest(request=request):
                    self.assertFalse(self.child.dispatch(request))
            kill.assert_not_called()
        self.assertEqual(self.child.last_token, 0)
        self.assertTrue(self.child.dispatch(p))

    def test_deadline_expires_during_observation_never_signals(self):
        p = self.center.request_lifecycle(self.child.key, "suspend")
        with patch("exodus.task_center_child.time.monotonic",
                   side_effect=[p.deadline - 1, p.deadline]), \
                patch("exodus.task_center_child.os.kill") as kill:
            self.assertFalse(self.child.dispatch(p))
            kill.assert_not_called()
        self.assertEqual(self.child.last_token, p.token)
        self.assertFalse(self.child.dispatch(p))

    def test_lost_reaper_never_signals_pid(self):
        p = self.center.request_lifecycle(self.child.key, "suspend")
        # Simulate lost child authority without losing the test fixture cleanup.
        with patch("exodus.task_center_child.os.waitpid", side_effect=ChildProcessError), \
                patch("exodus.task_center_child.os.kill") as kill:
            self.assertFalse(self.child.dispatch(p))
            self.assertEqual(self.child.state, UNKNOWN)
            self.assertTrue(self.child.terminal)
            kill.assert_not_called()
        self.child.terminal = False


if __name__ == "__main__":
    unittest.main()
