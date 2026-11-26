"""Validate Tumor's integration DAG, without treating recipes as proven builds."""
import json
from pathlib import Path

graph = json.loads(Path(__file__).with_name("graph.json").read_text())
assert graph["name"] == "EXODUS Tumor"
nodes = {node["id"]: node for node in graph["nodes"]}
assert len(nodes) == len(graph["nodes"]), "duplicate node ID"
visiting, done = set(), set()
order = []
def visit(key):
    assert key in nodes, f"unknown dependency: {key}"
    assert key not in visiting, f"cycle at {key}"
    if key in done:
        return
    visiting.add(key)
    for dependency in nodes[key]["depends"]:
        visit(dependency)
    visiting.remove(key)
    done.add(key)
    order.append(key)
for key, node in nodes.items():
    visit(key)
    if node["state"].endswith(("-verified", "-tested")):
        assert isinstance(node["evidence"]["run"], int)
        assert node["evidence"]["scope"]
print("Integration order (not a transitive Ports dependency closure):")
for key in order:
    print(f"{key}: {nodes[key]['state']}")
print(f"PASS: {len(order)} nodes")

# Physical admission must not be satisfied by reference-only nodes.
def ancestors(key):
    result = set()
    pending = list(nodes[key]["depends"])
    while pending:
        dep = pending.pop()
        if dep not in result:
            result.add(dep)
            pending.extend(nodes[dep]["depends"])
    return result
required = {"adsp-freebsd-boot", "a57-execution-substrate", "amp-syscall-transport",
            "a57-bsd-userspace", "horizon-multidriver", "horizon-minimal-backend",
            "resident-network"}
assert required <= ancestors("amp-boot-mvp")
assert {"amp-boot-mvp", "steam-hos-compatibility", "inbox-import", "desktop-mode"} <= ancestors("physical-release-admission")
assert not any(nodes[k]["state"].startswith("qemu-") for k in required)
assert graph["physical_target"]["topology"] == "AMP"
print("PASS: physical AMP admission dependency separation")
