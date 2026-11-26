# EXODUS EL2 monitor — reference stage 1

This isolated bare-metal AArch64 reference uses hardware EL2 on QEMU virt
Cortex-A57. It is not software elevation from EL1 and not a Switch boot payload.
It guards CurrentEL before privileged configuration, installs EL2 vectors,
sets HCR_EL2.RW/TGE, enters a trusted EL0 fixture via ERET, handles SVC64,
returns a synthetic task ID, rejects unknown opcodes and validates selected
register preservation and exit intent. With virtualization disabled, it must
report EL1_BLOCKED, never attempt EL2 system register access.

The reference uses one core, no MMU, no stage-2 mappings, no untrusted guests,
no scheduler, no interrupts, no actual FreeBSD syscalls, no AMP1 transport,
and no Horizon. TGE is for this standalone EL0 reference, NOT a global setting
to apply while Horizon runs. QEMU UART and link address are reference-only.

Next: isolated address spaces, local exception context, a bounded AMP1 adapter
and actual FreeBSD authority; separately investigate a supported boot-time EL2
handoff and Horizon EL1 guest coexistence, including interrupt/device ownership.
Reuse existing Bao evidence as the dual-guest reference, not as physical proof.
If authorized EL2 entry is unavailable on the target, hardware admission
remains blocked; no security bypass or firmware change is included.

Refs #103 #105 #349 #350. Preserve all prior AMP1 and ADSP work.
