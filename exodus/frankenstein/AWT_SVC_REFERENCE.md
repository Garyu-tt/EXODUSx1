# R15.8 A57 wait4 syscall/copyout reference
Independent A57 EL0 ELF calls actual FreeBSD ABI syscall number 7 with
exact child PID, WNOHANG and null rusage. EL1 relays to EL2, which validates
arguments and rejects an executable-page status destination with EFAULT
without sending a wire frame. Allowed copyout is exactly VA 0x5008 in
the owning guest's private writable ELF page, not arbitrary page walking.
AWT1 reply from synthetic ARM32 QEMU-user authority is matched to the
request; the monitor copies four status bytes, returns PID and clears
carry. Guest validates status, guard bytes, magic, then local fixture exit.
Eight lanes, eight rejected pointers, eight replies, 24 SVC, 16 HVC,
512 IRQ. This is intentionally not the native AWT1 kernel yet; R15.7
separately tests it. No generic wait4, rusage, real process reaping,
physical ADSP or production VM copyout. Fresh CI required.
