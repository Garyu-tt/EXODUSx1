import io
import unittest
from contextlib import redirect_stdout
from client import report, fields, plan, summary

BODY=b"schema=1\nkind=exodus_diagnostic_shell\ncommand=all\nadsp_execution=not_tested\na57_el2=not_tested\ndual_kernel=not_tested\ndeployment_authorized=0\nrepeat_safe=yes\n"

class ClientTests(unittest.TestCase):
    def test_good(self):
        self.assertEqual(report(io.BytesIO(b"REPORT 4\nabc\n")),b"abc\n")
        self.assertEqual(fields(BODY,"all")["repeat_safe"],"yes")
    def test_bad_frame(self):
        for raw in (b"REPORT 4\nabc",b"REPORT 9000\n",b"ERR command\n",b"REPORT -1\n",b"unknown\n",b"REPORT +4\nabc\n"):
            with self.subTest(raw=raw),self.assertRaises(ValueError):report(io.BytesIO(raw))
    def test_bad_fields(self):
        for raw in (BODY+b"schema=1\n",BODY[:-1],BODY.replace(b"authorized=0",b"authorized=1"),BODY.replace(b"command=all",b"command=cpu"),BODY.replace(b"repeat_safe=yes",b"repeat_safe=maybe")):
            with self.subTest(raw=raw),self.assertRaises(ValueError):fields(raw,"all")
    def test_plan(self):
        self.assertEqual(plan("help"),[])
        self.assertEqual(plan("repeat all 3"),["all"]*3)
        self.assertEqual(plan("cpu"),["cpu"])
        for s in ("repeat quit 2","repeat all 11","repeat all 1","ls","repeat cpu -2"):
            with self.subTest(s=s),self.assertRaises(ValueError):plan(s)
    def test_summary(self):
        out=io.StringIO()
        with redirect_stdout(out):summary([{"copy_integrity":"pass","copy_bytes":"4000","copy_ns":"1000"},{"copy_integrity":"pass","copy_bytes":"2000","copy_ns":"1000"},{"copy_integrity":"fail","copy_bytes":"9999","copy_ns":"1"}])
        self.assertIn("n=2/3 min=2.000 median=3.000 max=4.000",out.getvalue())
        self.assertIn("pass=2/3",out.getvalue())
