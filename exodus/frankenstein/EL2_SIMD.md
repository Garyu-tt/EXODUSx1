# R06: EL2 SIMD and selected EL1 thread state

Independent cooperative reference derived from R04, not dependent on timer IRQ. On QEMU Cortex-A57, save and restore 31 GPRs, ELR/SPSR, SP_EL0/SP_EL1, all 32 128-bit Q registers, FPCR, FPSR, TPIDR_EL0, TPIDRRO_EL0, TPIDR_EL1 and CONTEXTIDR_EL1. Context record 864 bytes, 16-byte aligned, vector state 512 bytes. Two trusted guests write different values into both lanes of every Q register and distinct FP control/status and thread IDs; verify on every iteration, 128 HVC yields and distinct data pages. Verify final saved SIMD context of both guests independently in EL2. FP/SIMD access explicitly enabled. EL1-only negative control.

This is not full architectural vCPU state: guest MMU registers, virtual interrupt/timer state, debug and PMU are not switched, CPACR/SCTLR are common fixed setup, no SMP, no real Horizon, no physical Switch. R04 and R06 require independent successful CI. Timer integration is a later gate. Refs #103 #105 #355 #356.
