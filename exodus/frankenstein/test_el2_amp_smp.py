"""Pass through AMP1 bytes; never generate authority replies."""
import os
import pathlib
import select
import subprocess
import sys
import tempfile
import time

def read_exact(p,n,deadline):
    out=bytearray()
    while len(out)<n:
        remaining=deadline-time.monotonic()
        if remaining<=0 or not select.select([p],[],[],remaining)[0]:
            raise TimeoutError(f"{len(out)}/{n}: {bytes(out)!r}")
        b=os.read(p.fileno(),n-len(out))
        if not b: raise EOFError(bytes(out))
        out.extend(b)
    return bytes(out)

def write_all(p,b):
    while b:
        n=os.write(p.fileno(),b)
        if n<=0: raise IOError("short write")
        b=b[n:]

def run(elf,authority):
    with tempfile.TemporaryDirectory() as d:
        trace=pathlib.Path(d)/"qemu.log"
        procs=[]
        try:
            for cmd in (["qemu-system-aarch64","-machine","virt,gic-version=2,virtualization=on","-cpu","cortex-a57","-smp","4","-m","128M","-display","none","-monitor","none","-chardev","stdio,id=amp,signal=off","-serial","chardev:amp","-net","none","-d","int,guest_errors","-D",str(trace),"-kernel",elf],
                        ["qemu-arm","-cpu","cortex-a9",authority]):
                procs.append(subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0))
            vm,peer=procs
            deadline=time.monotonic()+30
            seen={}
            for n in range(16):
                prefix=read_exact(vm.stdout,4,deadline)
                if prefix!=b"AMP1": raise AssertionError(f"request {n}: {prefix!r}")
                q=prefix+read_exact(vm.stdout,60,deadline)
                task=int.from_bytes(q[24:32],"little")
                assert 0x1234567887654301<=task<=0x1234567887654308
                i=seen.get(task,0)
                assert i<2 and q[5:7]==bytes((1,i+1))
                assert int.from_bytes(q[16:24],"little")==0x100000000+i
                seen[task]=i+1
                write_all(peer.stdin,q)
                r=read_exact(peer.stdout,64,deadline)
                assert r[:5]==b"AMP1\x01" and r[5]==2
                write_all(vm.stdin,r)
                print(f"relayed {n+1}/16 task={task:x}",flush=True)
            assert len(seen)==8 and all(v==2 for v in seen.values())
            marker=b"EXODUS_AMP_SMP_REF_PASS\n"
            assert read_exact(vm.stdout,len(marker),deadline)==marker
            assert peer.wait(timeout=5)==0
            assert peer.stderr.read().strip()==b"AMP_SMP_AUTHORITY_PASS width=32 lanes=8"
            vm.kill();vm.wait()
            log=trace.read_text(errors="replace")
            for core in range(4):
                assert log.splitlines().count(f"Taking exception 5 [IRQ] on CPU {core}")==128
            print("PASS: 8 lanes, 16 ARM32 replies, 512 timer IRQs")
        except Exception:
            for p in procs:
                if p.poll() is None:p.kill()
                p.wait()
                print(f"peer rc={p.returncode}, stderr={p.stderr.read()[-4096:]!r}",file=sys.stderr)
            if trace.exists():
                with trace.open("rb") as f:
                    f.seek(max(0,trace.stat().st_size-8192))
                    print(f.read().decode(errors="replace"),file=sys.stderr)
            raise
        finally:
            for p in procs:
                if p.poll() is None:p.kill()
                p.wait()
                for f in (p.stdin,p.stdout,p.stderr):f.close()

if __name__=="__main__":
    run(*sys.argv[1:])
