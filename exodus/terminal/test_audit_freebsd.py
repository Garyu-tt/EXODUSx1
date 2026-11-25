import json
import unittest
from audit_freebsd import LOCK, blob_id, plan

class AuditTests(unittest.TestCase):
    def setUp(self):
        self.lock = json.loads(LOCK.read_text(encoding="utf-8"))

    def test_order(self):
        order = plan(self.lock)
        self.assertEqual(len(order), len(self.lock["providers"]))
        for name, provider in self.lock["providers"].items():
            for dep in provider["requires"]:
                self.assertLess(order.index(dep), order.index(name))

    def test_cycle(self):
        self.lock["providers"]["build.generated"]["requires"] = ["shell.parser"]
        with self.assertRaisesRegex(ValueError, "cycle"):
            plan(self.lock)

    def test_bad_entry(self):
        self.lock["files"][0]["path"] = "../outside"
        with self.assertRaisesRegex(ValueError, "invalid donor"):
            plan(self.lock)

    def test_blob(self):
        self.assertEqual(blob_id(b""), "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391")

if __name__ == "__main__":
    unittest.main()
