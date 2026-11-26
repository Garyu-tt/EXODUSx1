# EL2 stage-2 MMU reference
Independent of R02 UART bridge. QEMU virt Cortex-A57 single trusted EL1 test
guest, stage-1 MMU off. EL2 builds three aligned translation tables, maps one
read-only executable page and one read/write XN page at guest IPAs distinct
from their physical backing. The rest of the address space is unmapped.
The guest must read/write its allowed page, then attempt read and write of
the unmapped EL2 image IPA. The monitor requires two ordered L3 translation
faults with the expected FAR/HPFAR, direction and lower-EL syndrome before
accepting HVC completion and verifying data and a monitor canary. EL1-only
entry must report blocked. CI reports exact marker and exception trace.

This is not a full security boundary or multi-guest scheduler. No stage-1
tables, real Horizon guest, DMA/SMMU, interrupts, cache ownership, actual
FreeBSD, Tegra startup or physical deployment. The guest code is trusted;
no claim of isolation from an arbitrary malicious guest. Test addresses
and UART belong only to QEMU virt. R02 remains a separate lane.
Refs #103 #105 #352 #353.
