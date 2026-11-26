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

R15.5 hardening: final EXIT_INTENT acknowledgement is separately authorized
by a kernel sysctl after process_exit evidence. Match the exact pending
frame, exit code and no signal, permit one commit only. The native relay
checks premature EAGAIN, wrong request EINVAL and duplicate EINVAL for
each of eight exits, forwards the committed kernel bytes unchanged.
Initial intent reply is provisional and never sent to A57 on exit.
This does not yet replace the native parent supervisor or implement remote
wait4. Fresh CI required.

R15.5 wait4 admission: EXIT_INTENT preparation yields 64 zero bytes, not a
valid AMP reply; only the native exit commit releases the original frame.
The actual parent uses wait4 with WNOHANG while each child is alive,
checks ECHILD for its own PID, reaps with wait4 and native rusage, then
checks ECHILD on repeat reaping. Final kernel frame is checked against
original request identity and forwarded unchanged. Eight wait4, eight
WNOHANG and sixteen ECHILD checks are required. This is native parent
wait4, not A57 syscall 7, remote status copyout or remote rusage.

Regression correction: FreeBSD 15.1 userland_sysctl internally retries
EAGAIN, so premature exit commit could block the sole supervisor before
it releases the child. Use EBUSY instead; verify it on every lane.
See https://reviews.freebsd.org/D60444. Preserve one-shot commit, no
increased timeout or synthetic fallback. Serial tail printed on failure.
