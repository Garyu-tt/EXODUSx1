#!/usr/bin/env python3
"""Boot stock ARMv7 FreeBSD under QEMU, invoke a native kernel module test."""
import pathlib
import re
import sys
import pexpect

disk,seed,bios,log=sys.argv[1:]
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
        child.sendline("shutdown -p now")
        child.expect(pexpect.EOF,timeout=180)
    finally:
        child.close(force=True)
data=pathlib.Path(log).read_text(errors="replace").replace("\r","")
for marker in ("EXODUS_ARMV7_KERNEL_AMP_LOADED","EXODUS_ARMV7_KERNEL_AMP_PASS lanes=8 transactions=16 replay=reject","EXODUS_ARMV7_KERNEL_UNLOAD_PASS","EXODUS_ARMV7_KERNEL_GATE_RESULT=0"):
    if len(re.findall(r"^"+re.escape(marker)+r"$",data,re.M))!=1:raise SystemExit("missing/duplicate "+marker)
if not re.search(r"^EXODUS_ARMV7_NATIVE_CALLER_PID_PASS pid=[1-9][0-9]*$",data,re.M):raise SystemExit("native PID evidence missing")
print("PASS: stock ARMv7 FreeBSD kernel handled 16 AMP1 frames")
