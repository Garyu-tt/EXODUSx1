import io
import unittest
from client import report

class ClientTests(unittest.TestCase):
    def test_good(self):
        self.assertEqual(report(io.BytesIO(b"REPORT 4\nabc\n")), b"abc\n")
    def test_bad(self):
        for raw in (b"REPORT 4\nabc", b"REPORT 9000\n", b"ERR command\n", b"REPORT -1\n", b"unknown\n"):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                report(io.BytesIO(raw))
