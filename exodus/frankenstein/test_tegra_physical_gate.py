import hashlib
import pathlib
import tempfile
import unittest
from tegra_physical_gate import GATES, evaluate

class Gate(unittest.TestCase):
    def test_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp)
            target={"soc":"mariko","board":"fixture-not-a-real-board"}
            data={"schema":1,"target":target,"gates":{}}
            self.assertEqual(evaluate(data,root)["decision"],"BLOCKED")
            f=root/"fixture.txt";f.write_text("synthetic test only")
            item={"kind":"qemu","target":target,"outcome":"pass","artifact":f.name,"sha256":hashlib.sha256(f.read_bytes()).hexdigest()}
            data["gates"]={g:dict(item) for g in GATES}
            self.assertEqual(evaluate(data,root)["decision"],"BLOCKED")
            for entry in data["gates"].values():entry["kind"]="physical"
            result=evaluate(data,root)
            self.assertEqual(result["decision"],"MANUAL_REVIEW_REQUIRED")
            self.assertFalse(result["deployment_authorized"])
            data["gates"][GATES[0]]["artifact"]="../outside"
            self.assertEqual(evaluate(data,root)["decision"],"BLOCKED")
            data["gates"][GATES[0]]["artifact"]=f.name
            f.write_text("changed")
            self.assertEqual(evaluate(data,root)["decision"],"BLOCKED")
            self.assertFalse(evaluate(data,root)["deployment_authorized"])
if __name__=="__main__":
    unittest.main()
