# R14: FreeBSD AArch64 syscall ABI subset

R13.1 runtime-loaded fixture now uses SVC #0, x8 syscall number, x0
argument and result, carry clear on success / set on positive errno.
SYS_getpid=20 maps a validated opaque AMP identity to a fixture-local
positive pid_t (1001..1008); unsupported number returns ENOSYS=78 locally.
SYS_exit=1 forwards exit intent and does not resume EL0 after acknowledgement.
The trusted test harness resumes EL1 to verify previous memory and SIMD checks;
this is not real process teardown. No extra fixture SVC is required.
Eight independent loaded ELF instances verify getpid, ENOSYS/carry, and
exit non-return. CI requires the same 24 SVCs, 16 HVCs and 16 replies.
Monitor ELF program headers now separate RX, R and RW; linker warnings fatal.
This does not itself enable EL2 stage-1 memory permissions.

Sources: FreeBSD arm64 cpu_set_syscall_retval:
https://cgit.freebsd.org/src/tree/sys/arm64/arm64/vm_machdep.c?id=91bfba010bcda665cc24a76af631cc85fcb0c688
Go FreeBSD arm64 syscall entry and generated syscall table:
https://go.googlesource.com/go.git/+/d3ffff2790059226ae3aa90856d687e138701b5c/src/syscall/asm_freebsd_arm64.s
https://go.googlesource.com/sys/+/master/unix/zsysnum_freebsd_arm64.go

Only direct syscall ABI subset, not FreeBSD libc, indirect syscall, real
kernel, ELF brand, actual pid management, generic userspace or physical
Switch. The authority remains a synthetic ARM32 process. Fresh CI required.
Refs #103 #105 #368.
