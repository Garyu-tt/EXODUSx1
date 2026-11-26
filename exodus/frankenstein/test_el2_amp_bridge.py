"""Relay AMP1 bytes; preserve failure diagnostics and exact rejection proof."""
import os
import pathlib
import select
import subprocess
import sys
import tempfile
import time

def read_exact(pipe,n,deadline):
    data=bytearray()
    while len(data)<n:
        remaining=deadline-time.monotonic()
        if remaining<=0 or not select.select([pipe],[],[],remaining)[0]:
            raise TimeoutError(f"peer frame {len(data)}/{n}: {data.hex()} {bytes(data)!r}")
        part=os.read(pipe.fileno(),n-len(data))
        if not part: raise EOFError(f"peer exited: {bytes(data)!r}")
        data.extend(part)
    return bytes(data)

def write_all(pipe,data):
    while data:
        n=os.write(pipe.fileno(),data)
        if n<=0: raise IOError("short write")
        data=data[n:]

def run(elf,authority,corrupt=False):
    peers=[]
    with tempfile.TemporaryDirectory() as d:
        trace=pathlib.Path(d)/"qemu.log"
        try:
            for cmd in (["qemu-system-aarch64","-machine","virt,virtualization=on","-cpu","cortex-a57","-smp","1","-m","128M","-display","none","-monitor","none","-chardev","stdio,id=amp,signal=off","-serial","chardev:amp","-net","none","-d","int,guest_errors","-D",str(trace),"-kernel",elf],
                        ["qemu-arm","-cpu","cortex-a9",authority,"authority"]):
                peers.append(subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0))
            vm,peer=peers
            deadline=time.monotonic()+45
            ready=b"EXODUS_EL2_AMP_READY\n"
            assert read_exact(vm.stdout,len(ready),deadline)==ready
            print("EL2 bridge initialized",flush=True)
            for i in range(1 if corrupt else 129):
                try:
                    prefix=read_exact(vm.stdout,4,deadline)
                    if prefix!=b"AMP1":
                        raise AssertionError(f"unexpected prefix {prefix!r}")
                    q=prefix+read_exact(vm.stdout,60,deadline)
                    assert q[:5]==b"AMP1\x01" and q[5]==1
                    assert int.from_bytes(q[16:24],"little")==0xffffffc0+i+1
                    write_all(peer.stdin,q)
                    r=read_exact(peer.stdout,64,deadline)
                    assert r[:5]==b"AMP1\x01" and r[5]==2
                    if corrupt:
                        r=r[:40]+bytes([r[40]^1])+r[41:]
                    write_all(vm.stdin,r)
                except Exception as exc:
                    raise RuntimeError(f"transaction {i+1}, corrupt={corrupt}") from exc
                if i in (0,63,127,128):
                    print(f"relayed transaction {i+1}",flush=True)
            marker=b"EXODUS_EL2_AMP_REJECT\n" if corrupt else b"EXODUS_EL2_AMP_PASS\n"
            assert read_exact(vm.stdout,len(marker),deadline)==marker
            if not corrupt:
                assert peer.wait(timeout=5)==0
                assert peer.stderr.read().strip()==b"AMP_PEER_PASS authority width=32 seq=4294967361"
            print(marker.decode().strip(),flush=True)
        except Exception:
            for p in peers:
                if p.poll() is None: p.kill()
                p.wait()
                print(f"peer rc={p.returncode}, stderr={p.stderr.read()[-4096:]!r}",file=sys.stderr)
            if trace.exists():
                with trace.open("rb") as f:
                    f.seek(max(0,trace.stat().st_size-8192))
                    print(f.read().decode(errors="replace"),file=sys.stderr)
            raise
        finally:
            for p in peers:
                if p.poll() is None: p.kill()
                p.wait()
                for f in (p.stdin,p.stdout,p.stderr): f.close()

if __name__=="__main__":
    run(*sys.argv[1:])
    run(*sys.argv[1:],corrupt=True)
    print("PASS: 129 EL0 requests and ARM32 replies, plus corrupt reply rejection")
