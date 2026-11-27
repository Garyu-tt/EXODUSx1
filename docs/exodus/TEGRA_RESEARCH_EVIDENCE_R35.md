# R35 — Actual FreeBSD Cortex-A9 reference boot experiment

R34 [inventory run 46032548927](https://github.com/Garyu-tt/EXODUSx1/actions/runs/46032548927) passed image verification; guest inventory still running at last observation.

FreeBSD releng/15.1 GENERIC blob 22bb759938349e28f6b27d5e3c03123d035707f6 selects ARMv7-A, CPU_CORTEXA, gic, generic_timer, mpcore_timer, pl011 and pl310. cpufunc.c blob ba63f82ee5976e383342828f6a18afd9a937e696 explicitly recognizes CPU_ID_CORTEXA9; mpcore_timer.c blob b4da9426128a5108aeb0ba197a785b71c8494330 lists Cortex-A9 global/private timer FDT compatibles.

R35 adds tegra-freebsd-a9-boot.yml (67c8cf1), [run 46032802094](https://github.com/Garyu-tt/EXODUSx1/actions/runs/46032802094). Pins upstream U-Boot v2025.01 commit 6d41f0a39d6423c8e57e92ebbe9f8c0333a63f72 and builds vexpress_ca9x4 with EFI boot enabled. Reuses verified release image, extends disposable copy to power-of-two SD size, attempts EFI loader from first FAT partition under QEMU vexpress-a9, one Cortex-A9, 1 GiB, network disabled and snapshot storage. Requires guest login and OS/CPU identity for PASS. FAT layout, EFI loader, DTB and drivers are experimental dependencies. No physical ADSP boot or Horizon coexistence claim. No console access. CI queued. Separate evidence file because append to R01 was rejected by connector.
