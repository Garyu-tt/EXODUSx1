"""EXODUS operator client R08; supports local approval and legacy NRO."""
import argparse
import getpass
import hashlib
import ipaddress
import re
import socket
import statistics
from pathlib import Path

COMMANDS = ("ping", "info", "cpu", "memory", "timer", "audio", "all", "quit")
HELP = "ping | probes | ls | put NAME SOURCE | cat NAME | run NAME | rm NAME | vm OPCODES... | info | cpu | memory | timer | audio | all | repeat COMMAND N (2-10) | repeat run NAME N | help | quit"

def report(stream):
    header = stream.readline(81)
    if header.startswith(b"ERR "):
        raise ValueError(header.decode("ascii", "replace").strip())
    if not re.fullmatch(rb"REPORT [1-9][0-9]{0,3}\n", header):
        raise ValueError("invalid header")
    size = int(header[7:])
    if size > 8192:
        raise ValueError("invalid size")
    body = stream.read(size)
    if len(body) != size:
        raise ValueError("truncated report")
    return body

def fields(body, command):
    if not body.endswith(b"\n"):
        raise ValueError("unterminated report")
    result = {}
    for line in body.decode("ascii").splitlines():
        k, sep, v = line.partition("=")
        if not sep or not re.fullmatch(r"[a-zA-Z0-9_]+", k) or k in result:
            raise ValueError("invalid or duplicate field")
        result[k] = v
    required = {"schema":"1","kind":"exodus_diagnostic_shell","command":command,
                "adsp_execution":"not_tested","a57_el2":"not_tested",
                "dual_kernel":"not_tested","deployment_authorized":"0"}
    if any(result.get(k)!=v for k,v in required.items()):
        raise ValueError("report identity mismatch")
    if result.get("repeat_safe") not in ("yes","no"):
        raise ValueError("missing repeat safety state")
    return result

def plan(line):
    words=line.split()
    if words==["help"]: return []
    if line in ("ls","probes") or re.fullmatch(r"(?:cat|run|rm) [A-Za-z0-9_-]{1,24}",line): return [line]
    if re.fullmatch(r"put [A-Za-z0-9_-]{1,24} [ -~]{1,768}",line): return [line]
    if words and words[0]=="vm" and 2<=len(words)<=193:
        if len(line)<=1023 and all(re.fullmatch(r"(?:[0-9]+|0x[0-9a-fA-F]+|0o[0-7]+|0b[01]+)",x) for x in words[1:]):
            return [" ".join(words)]
        raise ValueError("invalid VM program")
    if len(words)==1 and words[0] in COMMANDS: return words
    if len(words)==4 and words[:2]==["repeat","run"] and re.fullmatch(r"[A-Za-z0-9_-]{1,24}",words[2]) and re.fullmatch(r"[0-9]{1,2}",words[3]) and 2<=int(words[3])<=10:
        return ["run "+words[2]]*int(words[3])
    if len(words)==3 and words[0]=="repeat" and words[1] in COMMANDS[:-1]:
        if re.fullmatch(r"[0-9]{1,2}",words[2]) and 2<=int(words[2])<=10:
            return [words[1]]*int(words[2])
    raise ValueError("unknown command; enter help")

def summary(rows):
    for name, unit, values in [
        ("script", "us", [int(r["script_elapsed_ns"])/1000 for r in rows if r.get("fs_status")=="pass" and r.get("script_elapsed_ns","").isdigit()]),
        ("copy", "GB/s", [int(r["copy_bytes"])/int(r["copy_ns"]) for r in rows
                          if r.get("copy_integrity")=="pass" and r.get("copy_ns","").isdigit()
                          and int(r["copy_ns"])>0 and r.get("copy_bytes","").isdigit()]),
        *[(f"audio{n}", "ms", [int(r[f"audio{n}_elapsed_ns"])/1e6 for r in rows
                            if r.get(f"audio{n}")=="pass" and r.get(f"audio{n}_elapsed_ns","").isdigit()])
          for n in (1,4,8)]
    ]:
        if values:
            print(f"{name}: n={len(values)}/{len(rows)} min={min(values):.3f} median={statistics.median(values):.3f} max={max(values):.3f} {unit}")
    for key in ("parallel","copy_integrity","user_timer_progress","audio1","audio4","audio8"):
        vals=[r[key] for r in rows if key in r]
        if vals: print(f"{key}: pass={vals.count('pass')}/{len(vals)}, reported={len(vals)}/{len(rows)}")

def save(body, output):
    digest=hashlib.sha256(body).hexdigest()
    path=output/("TDA-"+digest+".txt")
    try:
        with path.open("xb") as out: out.write(body)
    except FileExistsError:
        if path.read_bytes()!=body: raise ValueError("existing report digest mismatch")
    print(digest,path)

def session(sock,stream,output):
    remaining=100
    print(HELP)
    while remaining:
        try:
            commands=plan(input("exodus> ").strip())
        except EOFError:
            return
        except ValueError as exc:
            print(exc);continue
        if not commands:
            print(HELP);continue
        if len(commands)>remaining:
            print(f"Only {remaining} server commands remain");continue
        rows=[]
        for cmd in commands:
            sock.sendall(cmd.encode()+b"\n")
            remaining-=1
            if cmd=="quit":return
            body=report(stream)
            row=fields(body,cmd)
            save(body,output)
            print(body.decode("ascii"))
            rows.append(row)
            if row["repeat_safe"]=="no":
                summary(rows)
                print("Server marked repeat unsafe; ending session")
                return
            if row.get("fs_status","pass")!="pass":
                print("Workspace command failed; stopping batch")
                break
        if len(rows)>1:summary(rows)

def pair(sock,stream):
    greeting=stream.readline(81)
    if greeting in (b"EXODUS-DIAG/2\n",b"EXODUS-DIAG/3\n"):
        print("Confirm the PC address on Switch with A (30 seconds).",flush=True)
    elif greeting==b"EXODUS-DIAG/1\n":
        token=getpass.getpass("Legacy Switch pairing code: ")
        if not re.fullmatch(r"[0-9a-f]{32}",token):
            raise ValueError("invalid code")
        sock.sendall(token.encode()+b"\n")
    else:
        raise ValueError("unexpected endpoint")
    if stream.readline(81)!=b"OK\n":
        raise ValueError("pairing rejected")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("ip",type=ipaddress.IPv4Address)
    p.add_argument("--output",type=Path,default=Path("."))
    args=p.parse_args()
    if not args.ip.is_private or args.ip.is_loopback or args.ip.is_multicast:
        p.error("use a trusted private LAN address")
    if not args.output.is_dir():p.error("output directory must exist")
    with socket.create_connection((str(args.ip),17491),timeout=10) as sock:
        sock.settimeout(60)
        with sock.makefile("rb") as stream:
            pair(sock,stream)
            session(sock,stream,args.output)

if __name__=="__main__":main()
