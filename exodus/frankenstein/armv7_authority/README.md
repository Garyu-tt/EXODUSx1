# R15.1: genuine ARMv7 FreeBSD kernel AMP1 admission
Boot stock 15.1-RELEASE GENERICSD under QEMU Cortex-A15 with U-Boot; image
SHA256 from official release announcement, source archive pinned to existing
15.1 source gate. Native build of ELF32 ARM KLD, load into FreeBSD kernel.
The kernel owns eight AMP1 sessions, processes frames via root-only sysctl
copyin/copyout and validates sequence/task, including 32-bit boundary.
Native ARMv7 client checks 16 transactions, malformed length, replay reject,
and separate caller PID diagnostic using the actual FreeBSD curproc PID.
Unload and verify sysctl removal. No arbitrary physical memory interface.
Transport and remote semantics remain synthetic: the native caller PID is
not a remote A57 PID, GETPID still echoes an opaque identity, EXIT_INTENT
only closes a session. This is an admission gate for real kernel code, not
live A57-to-FreeBSD IPC, ADSP port or a production split kernel. Stock
QEMU boot, compiler availability, KLD load and test all require fresh CI.
Next: transport bridge to R14 and real process ownership/lifecycle.
Refs #103 #105 #369. Official image:
https://www.freebsd.org/releases/15.1R/announce/

CI correction: first boot reached ttyu0 login, but the runner did not answer
the password prompt. Use the release-documented default root credential only
in the disposable network-disabled QEMU image, with bounded login steps and
explicit UID 0 evidence. No production credentials or authentication bypass.
Native module execution still awaits fresh CI.

Second CI correction: stock console authentication and UID 0 marker were
observed in the serial log, but the raw regex contained a double-escaped CR.
Match actual CR/LF (including repeated CR) and require a complete line in
both authentication and native result gates. Kernel module not yet tested.
