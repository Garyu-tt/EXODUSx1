# R38 — Direct real kernel boot without EFI

R37 U-Boot build and image verification passed; boot step still running.

FreeBSD releng/15.1 locore.S blob 2947032531b1396a70085f5db76f9d631cef31e7 accepts Linux ABI and infers section-aligned physical load base, sets identity mapping and maps kernel at its linked virtual base. machdep_boot.c blob 2eee28c7078b235db749fb7f730861dbe5565782 checks r2 DTB and synthesizes preload metadata. GENERIC enables LINUX_BOOT_ABI. EFI exec.c blob 71516569763d80e65273e809b53dc84322c476a5 translates ELF entry before calling it.

R38 commits 7e28314, fbe6f3e, fef21ef and b21f4bf create separate direct boot reference. Extracts official kernel.txz kernel, requires exact R34 audited SHA256 0c535d1cbe4640c676531b0a9fcca9eef704e008521d858541e72f9a88d2806d, checks five PT_LOAD ranges, relocates linked base to synthetic QEMU RAM 0x61000000 and enters 0x61000200 using a minimal ARM stub and synthetic vexpress DTB. No U-Boot or EFI; no disk or root filesystem. Gate is actual FreeBSD banner, not successful userspace. DTB completeness, timer frequency and compatibility are unverified. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46034625485. No physical access.
