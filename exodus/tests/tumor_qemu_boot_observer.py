"""Bounded QEMU HMP observation of disposable ARM kernel reference.
No target device, no network access, no guest writes.
"""
import bisect
import pathlib
import re
import socket
import subprocess
import sys
import time

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
sock = out / "monitor.sock"
serial = out / "direct-boot.log"
observation = out / "cpu-observation.log"
symbols = []
for line in (out / "kernel-symbols.log").read_text().splitlines():
    m = re.fullmatch(r"([0-9a-fA-F]+) [tTwW] (.+)", line)
    if m:
        symbols.append((int(m[1], 16), m[2]))
symbols.sort()
addresses = [a for a, _ in symbols]
def resolve_pc(pc):
    # The same kernel may execute at physical placement before MMU setup,
    # or its linked VA afterward. Only resolve within the audited RX range.
    va = pc + 0x5f000000 if 0x61000200 <= pc < 0x6174e600 else pc
    if not 0xc0000200 <= va < 0xc074e600:
        return f"PC={pc:#010x}: outside kernel RX segment"
    i = bisect.bisect_right(addresses, va) - 1
    if i < 0:
        return f"PC={pc:#010x} linked={va:#010x}: no symbol"
    a, name = symbols[i]
    return f"PC={pc:#010x} linked={va:#010x} nearest={name}+{va-a:#x}"

deadline = time.monotonic() + 75
with serial.open("wb") as log, observation.open("w") as evidence:
    p = subprocess.Popen([
        "qemu-system-arm", "-M", "vexpress-a9", "-cpu", "cortex-a9",
        "-m", "1024", "-smp", "1", "-display", "none", "-serial", "stdio",
        "-monitor", f"unix:{sock},server=on,wait=off", "-nic", "none",
        "-no-reboot", "-device", "loader,file=evidence/kernel.bin,addr=0x61000000,force-raw=on",
        "-device", "loader,file=evidence/board.dtb,addr=0x60040000,force-raw=on",
        "-device", "loader,file=handoff.bin,addr=0x60010000,cpu-num=0,force-raw=on"
    ], stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
    try:
        def prompt(s):
            data = bytearray()
            while b"(qemu)" not in data:
                part = s.recv(65536)
                if not part:
                    raise EOFError("QEMU monitor closed")
                data.extend(part)
                if len(data)>262144:
                    raise ValueError("monitor response too large")
            return data.decode(errors="replace")
        def command(s, cmd):
            s.sendall((cmd+"\n").encode())
            response = prompt(s)
            evidence.write(response+"\n")
            if cmd == "info registers":
                for match in re.finditer(r"\bR15\s*=\s*([0-9a-fA-F]{8})\b", response):
                    evidence.write(resolve_pc(int(match[1],16))+"\n")
            evidence.flush()
        for target in (0.2, 1, 2, 8, 20, 45, 65):
            until=deadline-75+target
            while time.monotonic()<until and p.poll() is None:
                time.sleep(0.2)
            if p.poll() is not None:
                break
            try:
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
                    s.settimeout(2)
                    s.connect(str(sock))
                    prompt(s)
                    evidence.write(f"\n--- sample at {target}s ---\n")
                    command(s,"info registers")
                    command(s,"info cpus")
            except (OSError, EOFError, ValueError) as exc:
                evidence.write(f"monitor observation unavailable: {exc}\n")
                evidence.flush()
    finally:
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
        evidence.write(f"QEMU status {p.returncode}\n")
        sock.unlink(missing_ok=True)
print(serial.read_text(errors="replace")[-24000:])
print(observation.read_text(errors="replace")[-24000:])
