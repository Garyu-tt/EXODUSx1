# R40 — Kernel transition diagnostics

R37 run 46033712413 job 140623686832 failed login timeout, but resolved the prior observed EFI abort: dcache ON, SCTLR 00c5187d, FreeBSD/arm EFI loader revision 3.0, UFS partition disk0p2, kernel and modules loaded, boot countdown ended, EFI DTB accepted, last output Loading shutdown splash ok. Kernel entry/liveness not established. QEMU trace artifact 15215239256 uploaded but not independently inspected.

R39 extraction/build step passed; direct boot pending.

R40 commits a6b3a10 and 7373655 introduce bounded QEMU HMP CPU observation (registers and CPUs at five points, no physical access), preserve exact verified kernel ELF, headers and symbol table for subsequent diagnosis and reuse. No automatic PC symbol resolution yet; no assertion that monitor samples or kernel banner have passed. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46035241552. No hardware interaction.
