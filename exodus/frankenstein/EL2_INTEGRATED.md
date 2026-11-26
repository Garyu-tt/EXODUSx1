# R09: integrated EL2 reference
A single QEMU virt image combines R05 timer/GICv2, R07 saved integer/SIMD and
selected EL1 translation state, and R08 four-core PSCI bootstrap. Each core
alternates two pinned EL1 fixtures with globally unique VMIDs/ASIDs, independent
stage-1 and stage-2 roots and data pages, common read-only code, per-core
monitor stacks and banked timer IRQs. Primary constructs all eight contexts
before release. Guests do not voluntarily yield; HVC is failure only.
32 timer IRQs per core, 128 total. Check per-core identity/progress, final
private data, TTBR0 and both lanes of all 32 saved SIMD registers for all eight
guests. Require exact per-core IRQ counts, success marker and EL1 negative control.

No real OS guest, no virtual GIC delivery to guests, no vCPU migration,
guest-writable stage-1 tables, dynamic scheduling, DMA/SMMU or physical
Horizon/ADSP proof. R02 AMP1 and R351 ADSP service reference are not yet
integrated into this image. Reference addresses and PSCI conduit QEMU-only.
Fresh CI required; no physical writes. Refs #103 #105 #353 #356 #359 #360.
