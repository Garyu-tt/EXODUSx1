"""Inspect a Board Probe R02 report; never grant physical admission."""
import argparse
import hashlib
import json
import pathlib
import re

MAX_BYTES = 1024
NUMERIC = {
    "chip_id": 255, "chip_major": 15, "chip_minor": 15,
    "hw_type": 255, "dram_id": 255, "dram_mib": 16383,
}
FIXED = {
    "schema": "1", "kind": "bpmp_board_inventory", "probe_revision": "2",
    "adsp_execution": "not_tested", "a57_el2": "not_tested",
    "dual_kernel": "not_tested", "deployment_authorized": "0",
}
FIELDS = set(NUMERIC) | set(FIXED) | {"hidrev"}


def inspect_report(raw):
    if not raw or len(raw) > MAX_BYTES:
        raise ValueError("invalid report size")
    fields = {}
    for line in raw.decode("ascii").splitlines():
        key, sep, value = line.partition("=")
        if not sep or key not in FIELDS or key in fields:
            raise ValueError("invalid or duplicate field: " + key)
        fields[key] = value
    if set(fields) != FIELDS:
        raise ValueError("incomplete report")
    if any(fields[k] != v for k, v in FIXED.items()):
        raise ValueError("unsupported revision or unverified execution claim")
    if not re.fullmatch(r"0x[0-9A-Fa-f]{8}", fields["hidrev"]):
        raise ValueError("invalid HIDREV")
    hidrev = int(fields["hidrev"], 16)
    numbers = {}
    for key, limit in NUMERIC.items():
        value = fields[key]
        if not re.fullmatch(r"0|[1-9][0-9]{0,4}", value) or int(value) > limit:
            raise ValueError("invalid numeric field: " + key)
        numbers[key] = int(value)
    if (numbers["chip_id"], numbers["chip_major"], numbers["chip_minor"]) != (
        (hidrev >> 8) & 255, (hidrev >> 4) & 15, (hidrev >> 16) & 15
    ):
        raise ValueError("HIDREV fields disagree")
    if numbers["chip_id"] != 0x21 or numbers["chip_major"] not in (1, 2):
        raise ValueError("unsupported Tegra identity")
    if numbers["dram_mib"] == 0:
        raise ValueError("zero configured DRAM")
    return {
        "kind": "board_inventory_review_only",
        "sha256": hashlib.sha256(raw).hexdigest(),
        "hidrev": fields["hidrev"],
        "observed": numbers,
        "soc": "mariko" if numbers["chip_major"] == 2 else "erista",
        "board_hint": {0: "icosa", 1: "iowa", 2: "hoag", 3: "aula"}.get(numbers["hw_type"], "unknown"),
        "physical_provenance_verified": False,
        "physical_gates_passed": [],
        "deployment_authorized": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=pathlib.Path)
    args = parser.parse_args()
    try:
        with args.report.open("rb") as f:
            raw = f.read(MAX_BYTES + 1)
        print(json.dumps(inspect_report(raw), indent=2))
    except (OSError, ValueError) as exc:
        parser.exit(2, str(exc) + "\n")


if __name__ == "__main__":
    main()
