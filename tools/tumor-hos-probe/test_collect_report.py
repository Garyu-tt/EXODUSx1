import unittest
from collect_report import extract

BODY = b"schema=1\nkind=hos_userland_inventory\nprobe_revision=4\nadsp_execution=not_tested\na57_el2=not_tested\ndual_kernel=not_tested\ndeployment_authorized=0\n"

def frame(body):
    return b"nxlink log\nTDA_REPORT_BEGIN %d\n" % len(body) + body + b"TDA_REPORT_END\n"

class TestCollector(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(extract(frame(BODY)), BODY)
        self.assertEqual(extract(frame(BODY).replace(b"\n", b"\r\n")), BODY)
    def test_bad(self):
        for bad in (frame(BODY)[:-8], frame(BODY)*2, frame(BODY+b"schema=1\n"),
                    frame(BODY.replace(b"not_tested",b"pass")), frame(BODY.replace(b"authorized=0",b"authorized=1")),
                    frame(BODY).replace(b"BEGIN ", b"BEGIN 9")):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                extract(bad)
