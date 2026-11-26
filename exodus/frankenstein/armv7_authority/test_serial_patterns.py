"""Regression: exact console markers from real QEMU PTY CR/CR/LF output."""
import ast
import pathlib
import re
import unittest

class SerialPatterns(unittest.TestCase):
    def test_exact_markers(self):
        tree=ast.parse(pathlib.Path(__file__).with_name("run.py").read_text())
        patterns=[n.args[0].value for n in ast.walk(tree)
                  if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute)
                  and n.func.attr=="expect" and n.args and isinstance(n.args[0],ast.Constant)
                  and isinstance(n.args[0].value,str) and "EXODUS_" in n.args[0].value]
        self.assertEqual(len(patterns),2)
        auth,result=map(re.compile,patterns)
        for eol in ("\n","\r\n","\r\r\n"):
            self.assertIsNotNone(auth.search("echo command"+eol+"EXODUS_CONSOLE_AUTH_OK"+eol+"root@generic:~ # "))
            self.assertEqual(result.search("EXODUS_ARMV7_KERNEL_GATE_RESULT=0"+eol).group(1),"0")
            self.assertEqual(result.search("EXODUS_ARMV7_KERNEL_GATE_RESULT=12"+eol).group(1),"12")
        self.assertIsNone(auth.search("echo EXODUS_CONSOLE_AUTH_OK\r\n"))
        self.assertIsNone(result.search("EXODUS_ARMV7_KERNEL_GATE_RESULT=0"))
        self.assertIsNone(result.search("EXODUS_ARMV7_KERNEL_GATE_RESULT=0BAD\r\n"))

if __name__=="__main__":
    unittest.main()
