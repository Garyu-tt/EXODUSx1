#!/usr/bin/env python3
"""Boot stock ARMv7 FreeBSD under QEMU, invoke a native kernel module test."""
import pathlib
import subprocess
import time
import re
import sys
import pexpect

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent))
from test_el2_freebsd_abi import read_exact, write_all

disk,seed,bios,log,elf,trace=sys.argv[1:]
cmd=["qemu-system-arm","-machine","virt","-cpu","cortex-a15","-m","1024",
     "-smp","1","-bios",bios,"-display","none","-serial","stdio","-monitor","none",
     "-no-reboot","-net","none","-drive",f"if=none,file={disk},format=raw,id=system",
     "-device","virtio-blk-device,drive=system",
     "-drive",f"if=none,file={seed},format=raw,id=seed,readonly=on",
     "-device","virtio-blk-device,drive=seed"]
with open(log,"w",encoding="utf-8") as f:
    child=pexpect.spawn(cmd[0],cmd[1:],encoding="utf-8",codec_errors="replace",timeout=900)
    child.logfile_read=f
    try:
        child.expect(r"login:",timeout=900)
        child.sendline("root")
        # Official 15.1 GENERICSD disposable image, network disabled.
        child.expect(r"Password:",timeout=30)
        child.sendline("root")
        outcome=child.expect([r"# ",r"Login incorrect",r"login:",r"Password:"],timeout=60)
        if outcome!=0:raise RuntimeError("stock image console authentication failed")
        child.sendline("sh")
        child.expect(r"# ",timeout=30)
        child.sendline("test $(id -u) -eq 0 && printf 'EXODUS_CONSOLE_%s\\n' AUTH_OK")
        child.expect(r"(?m)^EXODUS_CONSOLE_AUTH_OK\r*\n",timeout=30)
        child.expect(r"# ",timeout=30)
        child.sendline("mkdir -p /mnt/exodus-seed; mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed && cp /mnt/exodus-seed/guest.sh /var/tmp/exodus-guest.sh && umount /mnt/exodus-seed && sh /var/tmp/exodus-guest.sh")
        child.expect(r"(?m)^EXODUS_ARMV7_KERNEL_GATE_RESULT=([0-9]+)\r*\n",timeout=1800)
        if child.match.group(1)!="0":raise RuntimeError("native gate failed")
        child.expect(r"# ",timeout=30)
        child.sendline("mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed && cp /mnt/exodus-seed/amp2_guest.sh /var/tmp/exodus-live.sh && umount /mnt/exodus-seed && sh /var/tmp/exodus-live.sh")
        child.expect(r"(?m)^EXODUS_LIVE_READY pid=([1-9][0-9]*)\r*\n",timeout=180)
        print("native ARMv7 relay PID",child.match.group(1),flush=True)
        child.delaybeforesend=0
        # Both QEMU systems are alive here. Host only encodes/decodes hex
        # transport, never generates or edits an AMP response.
        vmcmd=["qemu-system-aarch64","-machine","virt,gic-version=2,virtualization=on",
               "-cpu","cortex-a57","-smp","4","-m","128M","-display","none","-monitor","none",
               "-chardev","stdio,id=amp,signal=off","-serial","chardev:amp","-net","none",
               "-d","int,guest_errors","-D",trace,"-kernel",elf]
        vm=subprocess.Popen(vmcmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
        try:
            deadline=time.monotonic()+30
            seen={}
            pids={}
            for n in range(16):
                q=read_exact(vm.stdout,64,deadline)
                assert q[:5]==b"AMP2\x02" and q[5]==1
                task=int.from_bytes(q[24:32],"little")
                assert 0x1234567887654301<=task<=0x1234567887654308
                i=seen.get(task,0)
                assert i<2 and q[6]==i+1
                assert int.from_bytes(q[16:24],"little")==0x100000000+i
                seen[task]=i+1
                child.sendline(q.hex())
                child.expect(r"(?m)^EXODUS_LIVE_REPLY ([0-9a-f]{128})\r*\n",timeout=3)
                reply=bytes.fromhex(child.match.group(1))
                assert reply[:5]==b"AMP2\x02" and reply[5]==2
                result=int.from_bytes(reply[40:48],"little")
                assert reply[7]==0
                if i==0:
                    assert 0<result<2**31 and result not in pids.values()
                    pids[task]=result
                else:assert result==0
                write_all(vm.stdin,reply)
                print(f"native kernel reply {n+1}/16 task={task:x}",flush=True)
            assert len(seen)==8 and all(v==2 for v in seen.values())
            marker=b"EXODUS_FREEBSD_ABI_REF_PASS\n"
            assert read_exact(vm.stdout,len(marker),deadline)==marker
            assert len(pids)==8
            child.expect(r"(?m)^EXODUS_AMP2_KERNEL_EXITED23=8\r*\n",timeout=30)
            child.expect(r"(?m)^EXODUS_LIVE_KERNEL_COUNT=16\r*\n",timeout=30)
            child.expect(r"(?m)^EXODUS_LIVE_UNLOAD_PASS\r*\n",timeout=30)
            child.expect(r"(?m)^EXODUS_LIVE_RESULT=0\r*\n",timeout=30)
        finally:
            if vm.poll() is None:vm.kill()
            vm.wait()
            for stream in (vm.stdin,vm.stdout,vm.stderr):stream.close()
        lines=pathlib.Path(trace).read_text(errors="replace").splitlines()
        for core in range(4):
            assert lines.count(f"Taking exception 5 [IRQ] on CPU {core}")==128
            assert lines.count(f"Taking exception 11 [Hypervisor Call] on CPU {core}")==4
            assert lines.count(f"Taking exception 2 [SVC] on CPU {core}")==6
        child.expect(r"# ",timeout=30)
        child.sendline("shutdown -p now")
        child.expect(pexpect.EOF,timeout=180)
    finally:
        child.close(force=True)
data=pathlib.Path(log).read_text(errors="replace").replace("\r","")
for marker in ("EXODUS_ARMV7_KERNEL_AMP_LOADED","EXODUS_ARMV7_KERNEL_AMP_PASS lanes=8 transactions=16 replay=reject","EXODUS_ARMV7_KERNEL_UNLOAD_PASS","EXODUS_ARMV7_KERNEL_GATE_RESULT=0"):
    if len(re.findall(r"^"+re.escape(marker)+r"$",data,re.M))!=1:raise SystemExit("missing/duplicate "+marker)
if not re.search(r"^EXODUS_ARMV7_NATIVE_CALLER_PID_PASS pid=[1-9][0-9]*$",data,re.M):raise SystemExit("native PID evidence missing")
print("PASS: 8 kernel-assigned virtual PIDs, 8 exit records, 16 native AMP2 replies, 512 IRQs")
