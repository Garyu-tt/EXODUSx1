# EL2 timer preemption reference (R05)

QEMU virt, Cortex-A57, one core, GICv2, CNTHP physical timer PPI 26.
Two trusted EL1 compute loops never voluntarily yield; the only guest HVC
is the failure path. Separate stage-2 roots, VMIDs and writable data pages.
Monitor requires exact timer IRQ identity and asserted timer status, disables
the timer before EOI, saves the integer context and arms a nominal 10 ms
quantum before ERET. Alternates guests for 32 actual timer interrupts; requires
both private counters advance, identity tags match, and saved counter vs memory
differ by at most one instruction-level increment. EL1-only negative control.
The reference is derived from R04; neither R04 nor R05 is promoted before
its own tests pass. No FP/SIMD, full EL1 system-register state, SMP, real
Horizon, FreeBSD or target GIC/timer/IRQ routing proof. Addresses are QEMU-only.
No target boot payload or firmware writes. Refs #103 #105 #354 #355.
