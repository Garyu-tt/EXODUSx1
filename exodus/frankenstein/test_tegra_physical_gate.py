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
    def test_dual_kernel_admission(self):
        extra = ["a57_el2_entry","a57_stage2_isolation","cross_kernel_irq_ownership","cross_kernel_memory_ownership","dual_kernel_liveness","native_game_bsd_ssh","display_hid_handoff"]
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            target = {"soc": "erista", "board": "synthetic-fixture"}
            artifact = root / "fixture.log"
            artifact.write_text("synthetic evidence only")
            item = {"kind": "physical", "target": target, "outcome": "pass",
                    "artifact": artifact.name,
                    "sha256": hashlib.sha256(artifact.read_bytes()).hexdigest()}
            data = {"schema": 1, "target": target,
                    "gates": {g: dict(item) for g in GATES if g not in extra}}
            result = evaluate(data, root)
            self.assertEqual(result["decision"], "BLOCKED")
            for gate in extra:
                self.assertEqual(result["gates"][gate],
                                 "BLOCKED: missing physical evidence")
                data["gates"][gate] = dict(item)
            self.assertEqual(evaluate(data, root)["decision"], "MANUAL_REVIEW_REQUIRED")
            for gate in extra:
                with self.subTest(gate=gate):
                    for field, value in (("kind", "qemu"),
                                         ("target", {"soc": "mariko", "board": "other"}),
                                         ("outcome", "fail")):
                        data["gates"][gate] = dict(item, **{field: value})
                        result = evaluate(data, root)
                        self.assertEqual(result["decision"], "BLOCKED")
                        self.assertFalse(result["deployment_authorized"])
                    data["gates"][gate] = dict(item)
            self.assertFalse(evaluate(data, root)["deployment_authorized"])
if __name__=="__main__":
    unittest.main()
