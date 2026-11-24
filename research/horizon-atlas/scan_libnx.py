#!/usr/bin/env python3
"""Offline inventory of pinned libnx checkout; syntactic candidates only."""
import argparse
import json
import pathlib
import re
import subprocess

PIN = "b156b4774fb2df7b93402169509266da7547fc4c"
COMMENTS = re.compile(r"/\*[\s\S]*?\*/|//[^\n]*")
PROTOTYPE = re.compile(
    r"(?m)^[ \t]*(?:[A-Za-z_]\w*[ \t*]+)+"
    r"([A-Za-z_]\w*)[ \t]*\(([^;{}]*?)\)[ \t]*(?:;|\{)"
)
GET_SERVICE = re.compile(r'\b(?:smGetService|smGetServiceOriginal)\s*\([^;\n]*?"([A-Za-z0-9:_-]{1,8})"')
DISPATCH = re.compile(r'\b(serviceDispatch(?:InOut|In|Out)?|serviceDispatchImpl|tipcDispatch(?:InOut|In|Out)?)\s*\(\s*([^,\n]+),\s*([0-9]+|0[xX][0-9A-Fa-f]+)\b')
IGNORE = {"if", "for", "while", "switch", "return", "sizeof", "defined"}

def blank_comments(src):
    return COMMENTS.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), src)

def scan_header(src):
    clean = blank_comments(src)
    return [{"name": m.group(1), "line": clean.count("\n", 0, m.start()) + 1}
            for m in PROTOTYPE.finditer(clean) if m.group(1) not in IGNORE]

def scan_source(src):
    clean = blank_comments(src)
    return {
        "literal_service_candidates": [
            {"name": m.group(1), "line": clean.count("\n", 0, m.start()) + 1}
            for m in GET_SERVICE.finditer(clean)],
        "literal_dispatch_candidates": [
            {"macro": m.group(1), "receiver_expression": m.group(2).strip(),
             "command": int(m.group(3), 16 if m.group(3).lower().startswith("0x") else 10),
             "line": clean.count("\n", 0, m.start()) + 1}
            for m in DISPATCH.finditer(clean)]
    }

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("checkout", type=pathlib.Path)
    p.add_argument("--output", type=pathlib.Path, required=True)
    p.add_argument("--revision", default=PIN)
    args = p.parse_args()
    root = args.checkout.resolve()
    actual = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    expected = subprocess.check_output(["git", "-C", str(root), "rev-parse", args.revision], text=True).strip()
    if actual != expected:
        p.error(f"checkout revision {actual} != expected {expected}")
    if subprocess.check_output(["git", "-C", str(root), "status", "--porcelain", "--untracked-files=all"], text=True).strip():
        p.error("checkout must be clean")
    records = []
    for base, suffixes in [("nx/include", {".h"}), ("nx/source", {".c", ".h", ".s", ".S"})]:
        for path in sorted((root / base).rglob("*")):
            if not path.is_file() or path.suffix not in suffixes:
                continue
            src = path.read_text(encoding="utf-8", errors="strict")
            entry = {"path": path.relative_to(root).as_posix(), "status": "syntactic_candidate_unreviewed"}
            if base == "nx/include":
                entry["declarations"] = scan_header(src)
            else:
                entry.update(scan_source(src))
            records.append(entry)
    if not records:
        p.error("no libnx files found")
    out = {"schema": "exodus.libnx-source-candidates/v1", "revision": actual,
           "warning": "Regex candidates only; macros, indirect calls, typedefs and preprocessor branches need AST and manual review. No endpoint/command pairing or runtime access inferred.",
           "counts": {"files": len(records), "declarations": sum(len(r.get("declarations", [])) for r in records),
                      "literal_services": sum(len(r.get("literal_service_candidates", [])) for r in records),
                      "literal_dispatches": sum(len(r.get("literal_dispatch_candidates", [])) for r in records)},
           "records": records}
    args.output.write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
