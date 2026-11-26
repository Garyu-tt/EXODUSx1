"""Relay bytes only between a real EL2 QEMU system and an ARM32 user-mode peer."""
import os
import select
import subprocess
import sys
import time

def read_exact(pipe,n,deadline):
    data=bytearray()
    while len(data)<n:
        remaining=deadline-time.monotonic()
        if remaining<=0 or not select.select([pipe],[],[],remaining)[0]:
            raise TimeoutError("peer frame")
        part=os.read(pipe.fileno(),n-len(data))
        if not part: raise EOFError("peer exited")
        data.extend(part)
    return bytes(data)

def run(elf,authority,corrupt=False):
    peers=[]
    try:
        for cmd in (["qemu-system-aarch64","-machine","virt,virtualization=on","-cpu","cortex-a57","-smp","1","-m","128M","-display","none","-monitor","none","-serial","stdio","-net","none","-kernel",elf],
                    ["qemu-arm","-cpu","cortex-a9",authority,"authority"]):
            peers.append(subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0))
        vm,peer=peers
        deadline=time.monotonic()+35
        for i in range(1 if corrupt else 129):
            q=read_exact(vm.stdout,64,deadline)
            assert q[:5]==b"AMP1\x01" and q[5]==1
            assert int.from_bytes(q[16:24],"little")==0xffffffc0+i+1
            peer.stdin.write(q)
            r=read_exact(peer.stdout,64,deadline)
            assert r[:5]==b"AMP1\x01" and r[5]==2
            if corrupt:
                r=r[:40]+bytes([r[40]^1])+r[41:]
            vm.stdin.write(r)
        marker=b"EXODUS_EL2_AMP_FAIL\n" if corrupt else b"EXODUS_EL2_AMP_PASS\n"
        assert read_exact(vm.stdout,len(marker),deadline)==marker
        if not corrupt:
            assert peer.wait(timeout=5)==0
            assert peer.stderr.read().strip()==b"AMP_PEER_PASS authority width=32 seq=4294967361"
        print(marker.decode().strip())
    finally:
        for p in peers:
            if p.poll() is None: p.kill()
            p.wait()
            for f in (p.stdin,p.stdout,p.stderr): f.close()

if __name__=="__main__":
    run(*sys.argv[1:])
    run(*sys.argv[1:],corrupt=True)
    print("PASS: 129 EL0 requests and ARM32 replies, plus corrupt reply rejection")
