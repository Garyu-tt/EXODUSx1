# R39 — Recover direct-kernel source from verified release SD image

R38 run 46034625485 / job 140626786688 failed before building handoff: requested kernel.txz URL HTTP 404. No direct A9 kernel execution occurred.

R37 boot step still running.

R39 commit 16d14f2b01e1b9fd2ff6e51aa7612717995280d3 replaces archive download with the previously verified release GENERICSD image (SHA256 and SHA512). Boots isolated disposable A15 reference VM with snapshot root disk and separate 64 MiB FAT32 export image. Copies /boot/kernel/kernel in guest, unmounts export disk, extracts with mtools and verifies exact R34 SHA256 before invoking R38 relocation plan. No network in VM, no physical storage or Switch access. Requires working FreeBSD virtio secondary block attachment and msdosfs support; both must be validated by CI. Direct A9 boot still requires kernel banner. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46034893500.
