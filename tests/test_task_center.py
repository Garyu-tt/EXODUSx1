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
        self.center.reconcile(None)
        self.assertEqual(self.center.begin(self.b.key).phase, "acquire")

    def test_wrong_observation_and_target_loss(self):
        self.center.reconcile(self.a.key)
        h = self.center.begin(self.b.key)
        with self.assertRaises(ValueError):
            self.center.acknowledge(h.token, "release", self.b.key)
        self.assertEqual(self.center.owner, UNKNOWN)
        self.center.reconcile(self.a.key)
        h = self.center.begin(self.b.key)
        self.center.acknowledge(h.token, "release", None)
        self.center.disconnect("freebsd")
        self.assertEqual(self.center.owner, UNKNOWN)
        self.assertIsNone(self.center.pending)

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

    def test_invalid_snapshot_is_atomic(self):
        for rows in ([self.a, self.a], [self.b], [Session("horizon", 1, "game", 0, "running")]):
            with self.assertRaises(ValueError):
                self.center.publish("horizon", 1, 2, rows)
        self.assertEqual(self.center.view()[0]["key"], self.a.key)
        self.center.publish("horizon", 1, 2, [self.a])


if __name__ == "__main__":
    unittest.main()
