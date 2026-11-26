"""Compare two exact EX-005 workload reports. No wall-time performance gate."""
import csv
import math
import statistics
import sys
from pathlib import Path

CASES = ("bdi_read", "bdi_fingerprint", "cold_patch", "hot_write")
META = {"fixture_bytes": 1289, "slab_payload_bytes": 1280, "slab_ref_bytes": 128}
FIELDS = ["case", "ticks", "operations", "tick_hz", "digest", "ns_per_op"]


def load(path):
    groups = {name: [] for name in CASES}
    meta = {}
    with Path(path).open(newline="", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        if reader.fieldnames != FIELDS:
            raise ValueError(f"{path}: unexpected CSV header")
        for row in reader:
            if None in row or None in row.values():
                raise ValueError(f"{path}: incomplete or extra fields")
            name = row["case"]
            ticks, operations, hz, digest = (
                int(row[key]) for key in FIELDS[1:5]
            )
            ns = float(row["ns_per_op"])
            if name in META:
                if name in meta or (operations, hz, digest, ns) != (0, 0, 0, 0):
                    raise ValueError(f"{path}: invalid metadata")
                meta[name] = ticks
                continue
            if (name not in groups or operations != 32768 or ticks <= 0
                    or hz <= 0 or not 0 <= digest < 2**64
                    or not math.isfinite(ns) or ns <= 0):
                raise ValueError(f"{path}: invalid workload row: {row}")
            actual = ticks * 1e9 / (operations * hz)
            if not math.isclose(ns, actual, rel_tol=1e-12, abs_tol=0.00051):
                raise ValueError(f"{path}: inconsistent timing")
            groups[name].append((digest, hz, actual))
    if meta != META:
        raise ValueError(f"{path}: incorrect fixture metadata")
    for name, rows in groups.items():
        if len(rows) != 7 or len({r[0] for r in rows}) != 1:
            raise ValueError(f"{path}: incomplete or inconsistent {name}")
    if len({r[1] for rows in groups.values() for r in rows}) != 1:
        raise ValueError(f"{path}: inconsistent timer frequency")
    return groups


def compare(baseline, candidate):
    old, new = load(baseline), load(candidate)
    print("case,baseline_median_ns,candidate_median_ns,baseline_over_candidate")
    for name in CASES:
        if old[name][0][:2] != new[name][0][:2]:
            raise ValueError(f"{name}: digest or timer frequency mismatch")
        a = statistics.median(r[2] for r in old[name])
        b = statistics.median(r[2] for r in new[name])
        print(f"{name},{a:.3f},{b:.3f},{a / b:.4f}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: compare_ex005_bench.py baseline.csv candidate.csv")
    compare(*sys.argv[1:])
