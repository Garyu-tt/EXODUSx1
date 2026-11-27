"""Bounded QEMU HMP observation of disposable ARM kernel reference.
No target device, no network access, no guest writes.
"""
import pathlib
import socket
import subprocess
import sys
import time

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
sock = out / "monitor.sock"
serial = out / "direct-boot.log"
observation = out / "cpu-observation.log"
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
            evidence.write(prompt(s)+"\n")
            evidence.flush()
        for target in (2, 8, 20, 45, 65):
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
