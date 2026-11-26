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
        child.sendline("mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed && cp /mnt/exodus-seed/awt_pid_guest.sh /var/tmp/exodus-live.sh && umount /mnt/exodus-seed && sh /var/tmp/exodus-live.sh")
        child.expect(r"(?m)^EXODUS_AWT_BOOT_IDS ([0-9a-f]{128})\r*\n",timeout=180)
        ids=bytes.fromhex(child.match.group(1))
        assert ids[:8]==b"AWB1\x01\x08\x00\x00"
        assert int.from_bytes(ids[8:16],"little")==0x1020304050607080 and not any(ids[48:])
        native=[int.from_bytes(ids[16+4*i:20+4*i],"little") for i in range(8)]
        assert len(set(native))==8 and all(0<p<2**31 for p in native)
        child.expect(r"(?m)^EXODUS_LIVE_READY pid=([1-9][0-9]*)\r*\n",timeout=30)
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
            hello=b'AWT_BOOT_READY\n'
            assert read_exact(vm.stdout,len(hello),deadline)==hello
            write_all(vm.stdin,ids)
            seen={}
            pids={}
            for n in range(8):
                q=read_exact(vm.stdout,64,deadline)
                assert q[:8]==b"AWT1\x01\x01\x01\x00"
                task=int.from_bytes(q[24:32],"little")
                i=task-0x1234567887654301
                assert 0<=i<8 and task not in seen
                assert int.from_bytes(q[32:40],"little")==9+i
                assert int.from_bytes(q[16:24],"little")==0x100000000
                seen[task]=1
                child.sendline(q.hex())
                child.expect(r"(?m)^EXODUS_LIVE_REPLY ([0-9a-f]{128})\r*\n",timeout=3)
                reply=bytes.fromhex(child.match.group(1))
                assert reply[:8]==b"AWT1\x01\x02\x01\x00" and reply[8:40]==q[8:40]
                pid=int.from_bytes(reply[40:44],"little")
                status=int.from_bytes(reply[44:48],"little")
                assert pid==native[i] and pid not in pids.values() and status==(23+i)<<8
                pids[task]=pid
                write_all(vm.stdin,reply)
                print(f"native AWT reply {n+1}/8 task={task:x} pid={pid} status={status}",flush=True)
            assert len(seen)==len(pids)==8
            marker=b"EXODUS_AWT_SVC_REF_PASS\n"
            assert read_exact(vm.stdout,len(marker),deadline)==marker
            child.expect(r"(?m)^EXODUS_LIVE_AWT_NATIVE_PASS children=8 replay=8\r*\n",timeout=30)
            child.expect(r"(?m)^EXODUS_LIVE_UNLOAD_PASS\r*\n",timeout=30)
            child.expect(r"(?m)^EXODUS_LIVE_RESULT=0\r*\n",timeout=30)
        except Exception:
            f.flush()
            print(pathlib.Path(log).read_text(errors="replace")[-12000:],file=sys.stderr)
            raise
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
print("PASS: 8 native boot PIDs, 8 A57 exact-PID wait4 fixtures, native AWT1 replies, protected copyouts, 512 IRQs")
