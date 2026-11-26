# R15.12 on-demand native proxy spawn
A57 EL0 issues private fixture syscall 4094, NOT FreeBSD SYS_fork.
AWS1 separate frame reserves a fixed slot in the native kernel. Only the
registered supervisor can reserve/commit; the real native child must
self-bind to its reserved slot. Early commit EBUSY, duplicate reserve
EBUSY, duplicate commit EINVAL. Kernel generates positive PID reply;
native supervisor verifies PID against fork result, forwards unchanged.
Then ordinary AWT1 exact-PID wait lifecycle: live zero, terminal PID/status,
ECHILD. No native child is created until an A57 request arrives.
32 wire replies, 48 SVC, 40 HVC, 512 IRQ expected. Fixed eight slots,
one trusted native supervisor, no A57 child context clone, no POSIX fork
semantics, no arbitrary executable, no physical Tegra. Fresh CI required.
