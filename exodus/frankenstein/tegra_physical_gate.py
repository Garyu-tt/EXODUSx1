#!/usr/bin/env python3
"""Offline evidence gate, not a hardware probe or authorization to flash."""
import argparse
import hashlib
import json
import pathlib
import re
import sys

GATES = (
    "board_revision", "adsp_cpu_features", "adsp_addressable_ram",
    "adsp_timer_irq", "reset_clock_ownership", "shared_memory_visibility",
    "dma_isolation", "horizon_audio_renderer", "horizon_audio_output",
    "horizon_opus", "suspend_resume", "stock_recovery",
    "a57_el2_entry", "a57_stage2_isolation", "cross_kernel_irq_ownership",
    "cross_kernel_memory_ownership", "dual_kernel_liveness",
    "native_game_bsd_ssh", "display_hid_handoff",
)
def evaluate(data, root):
    if not isinstance(data, dict) or data.get("schema") != 1:
        raise ValueError("invalid evidence schema")
    target=data.get("target")
    if not isinstance(target, dict) or target.get("soc") not in ("erista","mariko") or not isinstance(target.get("board"),str) or not target["board"].strip():
        raise ValueError("explicit target identity required")
    gates=data.get("gates")
    if not isinstance(gates,dict) or set(gates)-set(GATES):
        raise ValueError("invalid gate inventory")
    result={}
    for name in GATES:
        item=gates.get(name)
        if item is None:
            result[name]="BLOCKED: missing physical evidence"
            continue
        if not isinstance(item,dict):
            raise ValueError("invalid gate record: "+name)
        if item.get("kind")!="physical" or item.get("target")!=target:
            result[name]="BLOCKED: nonphysical or mismatched target"
            continue
        if item.get("outcome") not in ("pass","fail"):
            result[name]="BLOCKED: no observed outcome"
            continue
        path=item.get("artifact")
        digest=item.get("sha256")
        if not isinstance(path,str) or not re.fullmatch(r"[0-9a-f]{64}",digest if isinstance(digest,str) else ""):
            result[name]="BLOCKED: missing artifact identity"
            continue
        candidate=(root/path).resolve()
        if pathlib.Path(path).is_absolute() or not candidate.is_relative_to(root.resolve()) or not candidate.is_file():
            result[name]="BLOCKED: artifact unavailable"
            continue
        h=hashlib.sha256()
        with candidate.open("rb") as f:
            for chunk in iter(lambda:f.read(65536),b""):
                h.update(chunk)
        if h.hexdigest()!=digest:
            result[name]="BLOCKED: artifact digest mismatch"
            continue
        result[name]="REVIEW" if item["outcome"]=="pass" else "FAIL: recorded physical failure"
    # Provenance and content require independent manual review. Never turn
    # user-supplied JSON or a matching checksum into hardware acceptance.
    return {"target":target,"gates":result,"decision":"BLOCKED" if any(v!="REVIEW" for v in result.values()) else "MANUAL_REVIEW_REQUIRED","deployment_authorized":False}
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest",type=pathlib.Path)
    args=parser.parse_args()
    try:
        data=json.loads(args.manifest.read_text(encoding="utf-8"))
        print(json.dumps(evaluate(data,args.manifest.parent),indent=2))
    except (ValueError,OSError) as exc:
        print(str(exc),file=sys.stderr);return 2
    return 0
if __name__=="__main__":
    raise SystemExit(main())
