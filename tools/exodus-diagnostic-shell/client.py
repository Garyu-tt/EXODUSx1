"""Operator client for one foreground EXODUS diagnostic session."""
import argparse
import getpass
import hashlib
import ipaddress
import socket
from pathlib import Path

COMMANDS = ("info", "cpu", "memory", "timer", "audio", "all", "quit")

def report(stream):
    header = stream.readline(81)
    if header.startswith(b"ERR "):
        raise ValueError(header.decode("ascii", "replace").strip())
    if not header.startswith(b"REPORT ") or not header.endswith(b"\n"):
        raise ValueError("invalid header")
    size = int(header[7:])
    if not 0 < size <= 8192:
        raise ValueError("invalid size")
    body = stream.read(size)
    if len(body) != size:
        raise ValueError("truncated report")
    return body

def main():
    p = argparse.ArgumentParser()
    p.add_argument("ip", type=ipaddress.IPv4Address)
    p.add_argument("--output", type=Path, default=Path("."))
    args = p.parse_args()
    if not args.ip.is_private or args.ip.is_loopback or args.ip.is_multicast:
        p.error("use a trusted private LAN address")
    with socket.create_connection((str(args.ip),17491),timeout=10) as sock:
        sock.settimeout(60)
        with sock.makefile("rb") as stream:
            if stream.readline(81)!=b"EXODUS-DIAG/1\n":
                raise ValueError("unexpected endpoint")
            token=getpass.getpass("Switch pairing code: ")
            if len(token)!=32 or any(c not in "0123456789abcdef" for c in token):
                raise ValueError("invalid code")
            sock.sendall(token.encode()+b"\n")
            if stream.readline(81)!=b"OK\n":
                raise ValueError("pairing rejected")
            print("Commands:", ", ".join(COMMANDS))
            for n in range(100):
                cmd=input("exodus> ").strip()
                if cmd not in COMMANDS:
                    print("Unknown command");continue
                sock.sendall(cmd.encode()+b"\n")
                if cmd=="quit":break
                body=report(stream)
                print(body.decode("ascii", "replace"))
                digest=hashlib.sha256(body).hexdigest()
                path=args.output/("TDA-"+digest+".txt")
                if not path.exists():
                    with path.open("xb") as out:out.write(body)
                print(digest, path)

if __name__=="__main__":main()
