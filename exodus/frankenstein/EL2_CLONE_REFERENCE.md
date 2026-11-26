# R15.13 A57 clone reference
Independent single-core QEMU gate. EL0 private fixture SVC 4093 goes
through EL1 HVC to EL2. Monitor snapshots 31 GPRs, ELR_EL1, SPSR_EL1
and SP_EL0, copies the 4KiB private RW data/stack page, switches to a
separate stage-1/stage-2 root with distinct ASID/VMID and resumes child
at same post-SVC PC with x0=0. Child changes its own page, terminates
via fixture operation, parent resumes saved state with synthetic x0=2001.
Both verify stack and register sentinels; EL2 checks parent/child writes
remain independent. RX code is shared read-only. EL1-only boot fails
closed. No SIMD clone, COW, FD duplication, real PID or integration with
AWS1 yet; no real fork ABI or physical Tegra claim. Fresh CI required.
