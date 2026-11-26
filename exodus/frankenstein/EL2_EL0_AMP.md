# R12: EL0 SVC to EL1 HVC to ARM32 reference
Derived from R11. Eight pinned contexts on four A57 cores. Separate EL0
read-only executable page, PXN; separate EL1 vector page, UXN; existing EL1
code/data pages not accessible at EL0. Stage-1/2 mappings remain fixed.
Trusted EL1 fixture enters EL0 via ERET. EL0 executes two SVC #0x44
requests, validates results in x8, and uses SVC #0x45 to return through a
fixed trusted continuation. EL1 vector checks lower exception mode and
syndrome, then relays via HVC. EL2 checks HVC origin, saved SVC syndrome,
EL0 mode and exact trusted HVC return address before accepting request.
Existing wait/resume implementation restores saved result and EL1 state.
CI requires 24 SVC exceptions, 16 HVCs, 16 external ARM32 replies, 512
timer IRQs, all eight completion flags and previous state checks.
Reference ABI only; no genuine FreeBSD syscall ABI, ELF loader, generic
user stack, process termination, hostile-guest security proof, real
Horizon or physical ADSP. EL0 stub is trusted test code. SVC #0x45 is a
fixture-only continuation and must not be exposed in a production ABI.
No physical writes. Refs #103 #105 #364.

CI correction: GNU as rejected a cross-section literal expression for the
trusted HVC return address. Compute the offset at EL2 using two ADRs and
translate it into the fixed guest VA instead; the exact-address check remains.
Execution proof is pending fresh CI.
