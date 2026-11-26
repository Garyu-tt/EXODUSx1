# R10: asynchronous AMP1 lanes in integrated EL2 reference

R09 integrated QEMU image with eight pinned guests on four Cortex-A57 cores.
Each guest owns an AMP1 session and one bounded request/reply slot. CPU0
alone owns the UART transport, servicing at most one frame per timer tick;
other CPUs only publish and consume their own slots using acquire/release.
No busy-wait for UART in the timer handler, no global lock or extra dedicated
core. Deadline for active frame is 500 ms. Two synthetic operations per lane,
GETPID and EXIT_INTENT, cross the 32-bit sequence boundary. Separate Cortex-A9
user-mode authority process handles all eight sessions; host relays bytes only.
Require all lanes closed, 16 replies, distinct task identity, 128 timer IRQs
per core and existing R09 private RAM, SIMD and TTBR0 checks.

This is timer-driven monitor-side synthetic request submission, not actual
guest syscall forwarding. UART transport is QEMU-only; no physical shared
RAM coherence, real ADSP, FreeBSD, Horizon or guest device virtualization.
One failed lane fails the reference as a whole; production failure isolation
and guest wait/wake semantics remain future gates. R10 not locally executed.
Refs #103 #105 #353 #361.
