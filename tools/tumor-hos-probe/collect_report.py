"""Extract one complete report from a local NXLink log; never execute its contents."""
import argparse
import hashlib
import re
from pathlib import Path


def extract(raw: bytes) -> bytes:
    raw = raw.replace(b"\r\n", b"\n")
    starts = list(re.finditer(rb"(?m)^TDA_REPORT_BEGIN ([0-9]{1,5})\n", raw))
    if len(starts) != 1:
        raise ValueError("expected one report header")
    start = starts[0]
    size = int(start.group(1))
    if not 0 < size <= 8192:
        raise ValueError("report size invalid")
    report = raw[start.end():start.end() + size]
    if len(report) != size or not raw[start.end() + size:].startswith(b"TDA_REPORT_END\n"):
        raise ValueError("incomplete report")
    fields = {}
    for line in report.splitlines():
        key, sep, val = line.partition(b"=")
        if not sep or not re.fullmatch(rb"[a-zA-Z0-9_]+", key) or key in fields:
            raise ValueError("invalid or duplicate field")
        fields[key] = val
    required = {b"schema": b"1", b"kind": b"hos_userland_inventory",
                b"probe_revision": b"4", b"deployment_authorized": b"0",
                b"adsp_execution": b"not_tested", b"a57_el2": b"not_tested",
                b"dual_kernel": b"not_tested"}
    if any(fields.get(k) != v for k, v in required.items()):
        raise ValueError("unexpected report identity")
    return report


def main():
    p = argparse.ArgumentParser()
    p.add_argument("log", type=Path)
    p.add_argument("output", type=Path)
    args = p.parse_args()
    with args.log.open("rb") as stream:
        raw = stream.read(1048577)
    if len(raw) > 1048576:
        raise ValueError("log too large")
    report = extract(raw)
    with args.output.open("xb") as stream:
        stream.write(report)
    print(hashlib.sha256(report).hexdigest(), len(report), args.output)


if __name__ == "__main__":
    main()
