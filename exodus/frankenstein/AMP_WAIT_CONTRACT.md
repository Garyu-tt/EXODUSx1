# R15.6: remote wait wire admission
AWT1 is a separate 64-byte little-endian message (not an extension of AMP2).
Supports only exact child identity and WNOHANG; generation/sequence/parent
bound to trusted session. No raw addresses or native struct rusage. Native
exit adapter must supply authenticated exit evidence, not arbitrary user
status. Caller serializes registration/exit/wait; no live reset. Parent may
reap a terminal child once; unrelated or already reaped children return
ECHILD. A live child returns PID 0. Normal and terminal signal statuses
use POSIX wait encoding. Core dump, stopped, continued, negative PID,
wait-any, blocking waits and status copyout/rusage are out of scope.
The host test covers sequence crossing 32-bit, ownership and replay, corrupt
identity, 64 truncated lengths and normal/signal terminal statuses.
CI tests native ASan/UBSan and QEMU user-mode ARMv7/AArch64. This is a
portable contract, not a native FreeBSD or A57 syscall implementation.
Next: integrate authenticated process_exit source and protected A57
status copyout, then admission of real remote wait4.
Refs #103 #105 #373 #374.
