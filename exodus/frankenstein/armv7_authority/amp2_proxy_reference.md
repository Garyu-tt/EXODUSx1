# R15.5: AMP2 native proxy lifecycle
Stacked on R15.4, which still awaits CI. Uses unchanged A57 AMP2 code.
Eight native FreeBSD forked workers bind their own struct proc identity in
a disposable KLD. GETPID returns that native PID; host verifies uniqueness.
EXIT_INTENT closes the AMP session and creates an intent acknowledgement
inside the kernel. The native parent releases the corresponding child with
the requested exit code, calls waitpid, checks the kernel exit hook snapshot,
and forwards the original kernel reply only after both observations agree.
No user/host-generated reply. No external PID supplied, no arbitrary process
control. Actual A57 instructions still execute on A57, not the ARMv7 proxies.
Kernel hook owns actual exit evidence; parent supervises execution of exit.
This is not native remote fork/wait4/signal routing, nor a production process
namespace; it is a fixed eight-child admission. No physical Tegra claim.
CI required. Refs #103 #105 #372 #373.
