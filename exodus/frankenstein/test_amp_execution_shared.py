"""Real MAP_SHARED exchange across QEMU Linux user processes, no host relay."""
import pathlib
import struct
import subprocess
import sys
import tempfile

def run_pair(a, b, aw, bw, root):
    path = root / "mailbox"
    with path.open("xb") as f:
        f.truncate(256)
    subprocess.run(a + ["init", str(path)], check=True, timeout=10)
    peers = []
    try:
        for cmd, role in ((a, "executor"), (b, "authority")):
            peers.append(subprocess.Popen(cmd + [role, str(path)], stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE))
        for p, role, width in zip(peers, ("executor", "authority"), (aw, bw)):
            out, err = p.communicate(timeout=10)
            assert p.returncode == 0, (role, p.returncode, err)
            assert out.decode().strip() == f"AMP_SHARED_PASS {role} width={width} seq=4294967361"
            print(out.decode(), end="")
        raw = path.read_bytes()
        for offset in (0, 128):
            assert struct.unpack_from("<III", raw, offset) == (129, 129, 1)
        # No producer may reattach after its permanent claim, even after drain.
        rejected = subprocess.run(a + ["executor", str(path)], timeout=10, capture_output=True)
        assert rejected.returncode == 4
        assert path.read_bytes() == raw
    finally:
        for p in peers:
            if p.poll() is None:
                p.kill()
            p.wait()

if __name__ == "__main__":
    a,b = map(lambda s: str(pathlib.Path(s).resolve()), sys.argv[1:])
    arm32 = ["qemu-arm", "-cpu", "cortex-a9", a]
    arm64 = ["qemu-aarch64", "-cpu", "cortex-a57", b]
    for x,y,xw,yw in ((arm64,arm32,64,32),(arm32,arm64,32,64)):
        with tempfile.TemporaryDirectory() as d:
            run_pair(x,y,xw,yw,pathlib.Path(d))
    print("PASS: 516 AMP frames via shared memory; both role assignments; drained slots; duplicate attachment rejected")
