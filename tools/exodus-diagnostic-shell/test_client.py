import io
import unittest
from contextlib import redirect_stdout
from client import report, fields, plan, summary, pair
from unittest.mock import Mock, patch

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
        for s in ("repeat quit 2","repeat all 11","repeat all 1","cat ../x","repeat cpu -2"):
            with self.subTest(s=s),self.assertRaises(ValueError):plan(s)
    def test_summary(self):
        out=io.StringIO()
        with redirect_stdout(out):summary([{"copy_integrity":"pass","copy_bytes":"4000","copy_ns":"1000"},{"copy_integrity":"pass","copy_bytes":"2000","copy_ns":"1000"},{"copy_integrity":"fail","copy_bytes":"9999","copy_ns":"1"}])
        self.assertIn("n=2/3 min=2.000 median=3.000 max=4.000",out.getvalue())
        self.assertIn("pass=2/3",out.getvalue())

    def test_local_pair(self):
        sock=Mock()
        with redirect_stdout(io.StringIO()):
            pair(sock,io.BytesIO(b"EXODUS-DIAG/2\nOK\n"))
        sock.sendall.assert_not_called()
    def test_pair_denied(self):
        with redirect_stdout(io.StringIO()),self.assertRaises(ValueError):
            pair(Mock(),io.BytesIO(b"EXODUS-DIAG/2\nERR pairing\n"))
    def test_legacy_pair(self):
        sock=Mock()
        with patch("client.getpass.getpass",return_value="a"*32):
            pair(sock,io.BytesIO(b"EXODUS-DIAG/1\nOK\n"))
        sock.sendall.assert_called_once_with(b"a"*32+b"\n")

    def test_workspace_plan(self):
        for cmd in ("ls","put bench r0=core_mask(); r0*=0o7;","run bench","cat bench","rm bench"):
            self.assertEqual(plan(cmd),[cmd])
        for cmd in ("put ../file r0=1;","put x "+("a"*769),"put x a\nb"):
            with self.assertRaises(ValueError):plan(cmd)
    def test_protocol3_pair(self):
        sock=Mock()
        with redirect_stdout(io.StringIO()):
            pair(sock,io.BytesIO(b"EXODUS-DIAG/3\nOK\n"))
        sock.sendall.assert_not_called()

    def test_repeat_run(self):
        self.assertEqual(plan("repeat run bench 3"),["run bench"]*3)
        for cmd in ("repeat run ../x 3","repeat run bench 11","repeat run bench 0"):
            with self.assertRaises(ValueError):plan(cmd)

    def test_probe_inventory(self):
        self.assertEqual(plan("probes"),["probes"])

    def test_ping(self):
        self.assertEqual(plan("ping"),["ping"])
        self.assertEqual(plan("repeat ping 3"),["ping"]*3)
