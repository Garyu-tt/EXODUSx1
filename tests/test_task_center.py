import unittest

from exodus.task_center import TaskCenter, Session, UNKNOWN


class TaskCenterTests(unittest.TestCase):
    def setUp(self):
        self.now = 10
        self.center = TaskCenter(lambda: self.now)
        self.a = Session("horizon", 1, "game", 1, "running")
        self.b = Session("freebsd", 1, "game", 1, "running")
        self.center.publish("horizon", 1, 1, [self.a])
        self.center.publish("freebsd", 1, 1, [self.b])

    def test_backend_identity_and_unknown_owner(self):
        self.assertNotEqual(self.a.key, self.b.key)
        self.assertEqual(len(self.center.view()), 2)
        with self.assertRaises(ValueError):
            self.center.begin(self.a.key)

    def test_release_before_acquire(self):
        self.center.reconcile(self.a.key)
        handoff = self.center.begin(self.b.key)
        self.assertEqual(handoff.phase, "release")
        self.assertEqual(self.center.owner, self.a.key)
        with self.assertRaises(ValueError):
            self.center.acknowledge(handoff.token, "acquire", self.b.key)
        handoff = self.center.acknowledge(handoff.token, "release", None)
        self.assertIsNone(self.center.owner)
        self.assertEqual(handoff.phase, "acquire")
        self.center.acknowledge(handoff.token, "acquire", self.b.key)
        self.assertEqual(self.center.owner, self.b.key)
        with self.assertRaises(ValueError):
            self.center.acknowledge(handoff.token, "acquire", self.b.key)

    def test_disconnect_never_releases_device(self):
        self.center.reconcile(self.a.key)
        self.center.disconnect("horizon")
        self.assertEqual(self.center.owner, UNKNOWN)
        self.assertEqual(self.center.view()[0]["state"], UNKNOWN)
        with self.assertRaises(ValueError):
            self.center.begin(self.b.key)
        with self.assertRaises(ValueError):
            self.center.publish("horizon", 1, 2, [self.a])

    def test_timeout_and_explicit_recovery(self):
        self.center.reconcile(self.a.key)
        h = self.center.begin(self.b.key)
        self.now += 2
        with self.assertRaises(ValueError):
            self.center.acknowledge(h.token, "release", None)
        self.assertEqual(self.center.owner, UNKNOWN)
        with self.assertRaises(ValueError):
            self.center.reconcile(None)
        self.center.resolve_uncertain_handoff(h.token, h.phase, h.target, quiesced=True)
        self.assertEqual(self.center.owner, UNKNOWN)
        self.center.reconcile(None)
        self.assertEqual(self.center.begin(self.b.key).phase, "acquire")

    def test_wrong_observation_and_target_loss(self):
        self.center.reconcile(self.a.key)
        h = self.center.begin(self.b.key)
        with self.assertRaises(ValueError):
            self.center.acknowledge(h.token, "release", self.b.key)
        self.assertEqual(self.center.owner, UNKNOWN)
        self.center.resolve_uncertain_handoff(h.token, h.phase, h.target, quiesced=True)
        self.center.reconcile(self.a.key)
        h = self.center.begin(self.b.key)
        self.center.acknowledge(h.token, "release", None)
        self.center.disconnect("freebsd")
        self.assertEqual(self.center.owner, UNKNOWN)
        self.assertIsNone(self.center.pending)

    def test_timed_out_acquire_blocks_regrant_until_quiescence_and_reconcile(self):
        self.center.reconcile(None)
        h = self.center.begin(self.a.key)
        self.now += 2
        self.center.view()
        self.assertEqual(self.center.uncertain_handoff, h)
        for token, phase, target, quiesced in (
            (h.token + 1, h.phase, h.target, True),
            (h.token, "release", h.target, True),
            (h.token, h.phase, self.b.key, True),
            (h.token, h.phase, h.target, False),
            (True, h.phase, h.target, True),
        ):
            with self.assertRaises(ValueError):
                self.center.resolve_uncertain_handoff(token, phase, target, quiesced=quiesced)
        self.center.publish("horizon", 1, 2, [self.a])
        with self.assertRaises(ValueError):
            self.center.reconcile(None)
        with self.assertRaises(ValueError):
            self.center.begin(self.b.key)
        with self.assertRaises(ValueError):
            self.center.acknowledge(h.token, h.phase, h.target)
        self.center.resolve_uncertain_handoff(h.token, h.phase, h.target, quiesced=True)
        with self.assertRaises(ValueError):
            self.center.begin(self.b.key)
        self.center.reconcile(None)
        self.assertNotEqual(self.center.begin(self.b.key).token, h.token)

    def test_restart_rejects_old_identity(self):
        self.center.reconcile(self.a.key)
        new = Session("horizon", 2, "game", 1, "running")
        self.center.publish("horizon", 2, 1, [new])
        self.assertEqual(self.center.owner, UNKNOWN)
        with self.assertRaises(ValueError):
            self.center.reconcile(self.a.key)
        with self.assertRaises(ValueError):
            self.center.publish("horizon", 1, 99, [self.a])

    def test_expired_and_suspended_targets(self):
        suspended = Session("freebsd", 1, "game", 1, "suspended")
        self.center.publish("freebsd", 1, 2, [suspended])
        self.center.reconcile(None)
        with self.assertRaises(ValueError):
            self.center.begin(suspended.key)
        self.now += 3
        self.assertTrue(all(x["state"] == UNKNOWN for x in self.center.view()))

    def test_lifecycle_requires_capability_and_confirmed_background(self):
        self.center.reconcile(self.a.key)
        with self.assertRaises(ValueError):
            self.center.request_lifecycle(self.b.key, "suspend")
        supported = Session("freebsd", 1, "game", 1, "running", frozenset({"suspend"}))
        self.center.publish("freebsd", 1, 2, [supported])
        p = self.center.request_lifecycle(self.b.key, "suspend")
        with self.assertRaises(ValueError):
            self.center.begin(self.b.key)
        self.center.acknowledge_lifecycle(p.token, p.target, True)
        self.assertEqual(self.center.view()[1]["state"], "running")
        self.assertIsNotNone(self.center.lifecycle)
        self.center.publish("freebsd", 1, 3, [Session("freebsd", 1, "game", 1, "suspended")])
        self.assertEqual(self.center.lifecycle_result, (p.token, "confirmed"))
        self.assertIsNone(self.center.lifecycle)

    def test_lifecycle_no_confirmation_from_pre_acceptance_snapshot(self):
        s = Session("freebsd", 1, "game", 1, "suspended", frozenset({"resume"}))
        self.center.publish("freebsd", 1, 2, [s])
        self.center.reconcile(self.a.key)
        p = self.center.request_lifecycle(s.key, "resume")
        running = Session("freebsd", 1, "game", 1, "running")
        self.center.publish("freebsd", 1, 3, [running])
        self.center.acknowledge_lifecycle(p.token, p.target, True)
        self.assertIsNotNone(self.center.lifecycle)
        self.center.publish("freebsd", 1, 4, [running])
        self.assertEqual(self.center.lifecycle_result, (p.token, "confirmed"))

    def test_lifecycle_rejects_foreground_and_unknown(self):
        s = Session("horizon", 1, "game", 1, "running", frozenset({"terminate"}))
        self.center.publish("horizon", 1, 2, [s])
        with self.assertRaises(ValueError):
            self.center.request_lifecycle(s.key, "terminate")
        self.center.reconcile(s.key)
        with self.assertRaises(ValueError):
            self.center.request_lifecycle(s.key, "terminate")

    def test_lifecycle_loss_is_not_exit(self):
        s = Session("freebsd", 1, "game", 1, "running", frozenset({"terminate"}))
        self.center.publish("freebsd", 1, 2, [s])
        self.center.reconcile(None)
        p = self.center.request_lifecycle(s.key, "terminate")
        self.center.acknowledge_lifecycle(p.token, p.target, True)
        self.center.publish("freebsd", 1, 3, [])
        self.assertEqual(self.center.lifecycle_result, (p.token, UNKNOWN))
        with self.assertRaises(ValueError):
            self.center.acknowledge_lifecycle(p.token, p.target, True)

    def test_lifecycle_rejection_and_deadline(self):
        s = Session("freebsd", 1, "game", 1, "running", frozenset({"terminate"}))
        self.center.publish("freebsd", 1, 2, [s])
        self.center.reconcile(None)
        p = self.center.request_lifecycle(s.key, "terminate")
        self.center.acknowledge_lifecycle(p.token, p.target, False)
        self.assertEqual(self.center.lifecycle_result, (p.token, "rejected"))
        p = self.center.request_lifecycle(s.key, "terminate")
        self.center.acknowledge_lifecycle(p.token, p.target, True)
        self.now += 2
        self.center.publish("freebsd", 1, 3, [s])
        self.now += 2
        self.center.publish("freebsd", 1, 4, [s])
        self.now += 1
        self.center.view()
        self.assertEqual(self.center.lifecycle_result, (p.token, UNKNOWN))

    def test_uncertain_lifecycle_blocks_new_operations_until_adapter_quiescence(self):
        s = Session("freebsd", 1, "game", 1, "running", frozenset({"terminate"}))
        self.center.publish("freebsd", 1, 2, [s])
        self.center.reconcile(self.a.key)
        p = self.center.request_lifecycle(s.key, "terminate")
        self.center.acknowledge_lifecycle(p.token, p.target, True)
        self.center.disconnect("horizon")
        self.assertEqual(self.center.lifecycle_result, (p.token, UNKNOWN))
        self.assertEqual(self.center.owner, UNKNOWN)
        self.center.reconcile(None)
        self.center.publish("freebsd", 1, 3, [s])
        with self.assertRaises(ValueError):
            self.center.begin(s.key)
        with self.assertRaises(ValueError):
            self.center.request_lifecycle(s.key, "terminate")
        for token, target, quiesced in ((p.token+1, p.target, True),
                                        (p.token, self.a.key, True),
                                        (p.token, p.target, False)):
            with self.assertRaises(ValueError):
                self.center.resolve_uncertain_lifecycle(token, target, quiesced=quiesced)
        self.center.resolve_uncertain_lifecycle(p.token, p.target, quiesced=True)
        self.assertEqual(self.center.lifecycle_result, (p.token, UNKNOWN))
        self.assertEqual(self.center.begin(s.key).phase, "acquire")

    def test_timeout_without_ack_cannot_be_retried_after_fresh_snapshot(self):
        s = Session("freebsd", 1, "game", 1, "running", frozenset({"suspend"}))
        self.center.publish("freebsd", 1, 2, [s])
        self.center.reconcile(None)
        p = self.center.request_lifecycle(s.key, "suspend")
        self.now += 2
        self.center.publish("freebsd", 1, 3, [s])
        self.now += 2
        self.center.publish("freebsd", 1, 4, [s])
        self.now += 1
        self.center.view()
        self.assertEqual(self.center.lifecycle_result, (p.token, UNKNOWN))
        with self.assertRaises(ValueError):
            self.center.request_lifecycle(s.key, "suspend")
        self.center.resolve_uncertain_lifecycle(p.token, p.target, quiesced=True)
        self.assertNotEqual(self.center.request_lifecycle(s.key, "suspend").token, p.token)

    def test_boolean_identity_cannot_alias_integer_epoch(self):
        self.center.reconcile(None)
        with self.assertRaises(ValueError):
            self.center.begin(("horizon", True, "game", 1))
        with self.assertRaises(ValueError):
            self.center.reconcile(("horizon", 1, "game", True))

    def test_acknowledgment_rejects_boolean_alias(self):
        self.center.reconcile(None)
        h = self.center.begin(self.b.key)
        with self.assertRaises(ValueError):
            self.center.acknowledge(h.token, "acquire", ("freebsd", True, "game", 1))
        self.assertEqual(self.center.owner, UNKNOWN)

    def test_lifecycle_ack_and_quiescence_require_exact_typed_key(self):
        s = Session("freebsd", 1, "game", 1, "running", frozenset({"suspend"}))
        self.center.publish("freebsd", 1, 2, [s])
        self.center.reconcile(None)
        p = self.center.request_lifecycle(s.key, "suspend")
        alias = ("freebsd", 1, "game", True)
        with self.assertRaises(ValueError):
            self.center.acknowledge_lifecycle(p.token, alias, True)
        self.center.disconnect("freebsd")
        with self.assertRaises(ValueError):
            self.center.resolve_uncertain_lifecycle(p.token, alias, quiesced=True)
        self.assertIsNotNone(self.center.uncertain_lifecycle)

    def test_invalid_unhashable_values_fail_closed(self):
        with self.assertRaises(ValueError):
            self.center.publish([], 1, 1, [])
        with self.assertRaises(ValueError):
            self.center.disconnect([])
        with self.assertRaises(ValueError):
            self.center.publish("horizon", 1, 2, [Session("horizon", 1, "game", 1, [])])
        self.center.reconcile(None)
        with self.assertRaises(ValueError):
            self.center.request_lifecycle(self.b.key, [])
        self.center.publish("horizon", 1, 2, [self.a])

    def test_invalid_snapshot_is_atomic(self):
        for rows in ([self.a, self.a], [self.b], [Session("horizon", 1, "game", 0, "running")]):
            with self.assertRaises(ValueError):
                self.center.publish("horizon", 1, 2, rows)
        self.assertEqual(self.center.view()[0]["key"], self.a.key)
        self.center.publish("horizon", 1, 2, [self.a])


if __name__ == "__main__":
    unittest.main()
