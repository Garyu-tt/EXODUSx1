# R13.1: execute independently linked ELF through EXODUS EL0
Uses the R13 loader and R12 integrated monitor. Build a separate AArch64
ET_EXEC file, embed the unmodified file bytes as data, and parse/load at boot.
Eight exclusive backing windows, initially poisoned, independently zeroed
and loaded. Fixed reference layout: RX at VA 0x3000, RW/NX at 0x5000,
gap at 0x4000 is the existing privileged relay page (not ELF backing).
Require exact two-segment plan and entry before installing mappings.
The loaded ELF checks initialized data and zero BSS, writes unique guest
identity to BSS, submits both existing SVC requests and verifies replies,
then returns through trusted fixture continuation. EL2 independently
verifies eight private backing stores and prior SIMD/MMU/IRQ checks.
No embedded alternate user instruction path. Instruction cache maintenance
uses CTR_EL0 line sizes before releasing cores.

Not a generic dynamic mapper, process stack, ELF interpreter or FreeBSD ABI.
The fixed layout adapter accepts only this fixture shape. No arbitrary
guest process, physical hardware or real kernel claim. CI required.
Refs #103 #105 #365 #366.
