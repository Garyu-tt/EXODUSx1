"""Validate Tumor's integration DAG, without treating recipes as proven builds."""
import copy
import json
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


MVP_GATES = {
    "adsp-board-admission", "adsp-freebsd-boot", "a57-execution-substrate",
    "amp-syscall-transport", "a57-bsd-userspace", "horizon-multidriver",
    "horizon-minimal-backend", "resident-network", "dual-kernel",
    "adsp-native-service-contract", "adsp-native-compat-driver",
    "adsp-native-service-admission",
}
RELEASE_GATES = {"amp-boot-mvp", "steam-hos-compatibility", "inbox-import",
                 "desktop-mode", "sd-logical-layout"}


def validate(graph):
    require(graph["name"] == "EXODUS Tumor", "unexpected graph name")
    nodes = {node["id"]: node for node in graph["nodes"]}
    require(len(nodes) == len(graph["nodes"]), "duplicate node ID")
    visiting, done, order = set(), set(), []

    def visit(key):
        require(key in nodes, f"unknown dependency: {key}")
        require(key not in visiting, f"cycle at {key}")
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
            evidence = node.get("evidence", {})
            require(type(evidence.get("run")) is int and bool(evidence.get("scope")),
                    f"missing scoped run evidence: {key}")

    def ancestors(key):
        require(key in nodes, f"missing admission node: {key}")
        result, pending = set(), list(nodes[key]["depends"])
        while pending:
            dep = pending.pop()
            if dep not in result:
                result.add(dep)
                pending.extend(nodes[dep]["depends"])
        return result

    require(MVP_GATES <= ancestors("amp-boot-mvp"), "missing physical MVP dependency")
    require(RELEASE_GATES <= ancestors("physical-release-admission"),
            "missing physical release dependency")
    # This schema has no physical acceptance evidence format. Fail closed until
    # a separately reviewed schema defines it; reference statuses never qualify.
    for key in MVP_GATES | RELEASE_GATES | {"physical-release-admission"}:
        require(nodes[key]["state"] in {"needs-hardware-proof", "blocked"},
                f"physical gate {key} has inadmissible state: {nodes[key]['state']}")
    target = graph["physical_target"]
    require(target["topology"] == "AMP", "physical topology is not AMP")
    require(target["state"] in {"needs-hardware-proof", "blocked"},
            "physical target cannot claim acceptance in this schema")
    return nodes, order


def negative_tests(graph):
    def rejected(mutator, expected):
        candidate = copy.deepcopy(graph)
        nodes = {node["id"]: node for node in candidate["nodes"]}
        mutator(candidate, nodes)
        try:
            validate(candidate)
        except ValueError as error:
            require(expected in str(error), f"wrong rejection: {error}")
        else:
            raise ValueError(f"negative test accepted: {expected}")

    for state in ("source-audited", "freebsd-qemu-arm64-tested",
                  "static-syscall-and-lse-boundary-verified", "physical-verified"):
        def set_state(g, n):
            n["adsp-freebsd-boot"]["state"] = state
            n["adsp-freebsd-boot"]["evidence"] = {"run": 1, "scope": "synthetic"}
        rejected(set_state, "inadmissible state")
    rejected(lambda g, n: n["adsp-freebsd-boot"]["depends"].append("amp-boot-mvp"),
             "cycle at")
    rejected(lambda g, n: n["amp-boot-mvp"]["depends"].append("not-a-real-node"),
             "unknown dependency")
    rejected(lambda g, n: n["amp-boot-mvp"]["depends"].clear(),
             "missing physical MVP dependency")
    rejected(lambda g, n: n["physical-release-admission"]["depends"].clear(),
             "missing physical release dependency")
    rejected(lambda g, n: g["physical_target"].update(state="physical-verified"),
             "physical target cannot claim acceptance")
    rejected(lambda g, n: g["nodes"].append(copy.deepcopy(g["nodes"][0])),
             "duplicate node ID")
    print("PASS: 10 negative admission regressions")


def main():
    graph = json.loads(Path(__file__).with_name("graph.json").read_text())
    nodes, order = validate(graph)
    print("Integration order (not a transitive Ports dependency closure):")
    for key in order:
        print(f"{key}: {nodes[key]['state']}")
    print(f"PASS: {len(order)} nodes")
    print("PASS: physical AMP admission dependency separation")
    print("PASS: original ADSP functions are mandatory physical MVP gates")
    negative_tests(graph)


if __name__ == "__main__":
    main()
