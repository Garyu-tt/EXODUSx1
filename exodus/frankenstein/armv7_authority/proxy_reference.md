# R15.4: native process lifecycle admission
Independent next gate on top of R15.3. Actual FreeBSD ARMv7 forked workers
self-bind their own current struct proc identity to one of eight slots
through a root-only fixed sysctl. No arbitrary PID lookup/control. Kernel
process_exit event records exit code/signal and clears live pointer before
process destruction; no pointer returned to userspace. Duplicate binding
rejected, unload refused while a live slot exists. Native parent uses
waitpid on its own children, checks seven normal exits and one SIGTERM;
snapshot independently verifies native kernel lifecycle observations.
Source references: FreeBSD sys/kern/kern_exit.c process_exit event and
sys/sys/proc.h p_xexit/p_xsig. Test is a native proxy admission only:
not yet linked to AMP2, no remote fork/wait4/signal semantics, no A57
execution of proxy instructions, no production process namespace or
physical ADSP. Requires fresh CI. Refs #103 #105 #372.
