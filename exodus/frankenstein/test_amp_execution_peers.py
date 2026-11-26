"""Run two separate QEMU processes with direct bidirectional pipes.
The parent never implements or responds to AMP1 messages."""
import os
import pathlib
import subprocess
import sys
import time

def run(executor, authority, ew, aw):
    qr, qw = os.pipe()
    rr, rw = os.pipe()
    peers = []
    try:
        peers.append(subprocess.Popen(executor + ["executor"], stdin=rr, stdout=qw,
                                      stderr=subprocess.PIPE))
        peers.append(subprocess.Popen(authority + ["authority"], stdin=qr, stdout=rw,
                                      stderr=subprocess.PIPE))
    finally:
        for fd in (qr, qw, rr, rw):
            os.close(fd)
        if len(peers) != 2:
            for p in peers:
                p.kill()
                p.wait()
    try:
        deadline = time.monotonic() + 15
        for p, role, width in zip(peers, ("executor", "authority"), (ew, aw)):
            _, err = p.communicate(timeout=max(0.1, deadline-time.monotonic()))
            line = err.decode()
            print(line, end="")
            assert p.returncode == 0, (role, p.returncode)
            assert line.strip() == f"AMP_PEER_PASS {role} width={width} seq=4294967361"
    finally:
        for p in peers:
            if p.poll() is None:
                p.kill()
            p.wait()

if __name__ == "__main__":
    a, b = map(lambda s: str(pathlib.Path(s).resolve()), sys.argv[1:])
    arm32 = ["qemu-arm", "-cpu", "cortex-a9", a]
    arm64 = ["qemu-aarch64", "-cpu", "cortex-a57", b]
    run(arm64, arm32, 64, 32)
    run(arm32, arm64, 32, 64)
    print("PASS: 258 cross-ISA requests and 258 replies, both role assignments")
