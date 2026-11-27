# R55 — 32 MiB source-built ARMv7 FreeBSD single-user PASS

R53 build 46045173543 succeeded; source-built EXODUS_A9_REF exact SHA256 e32e0e0120720464ef0789e4e3d6d85fb823a34fb63d3e7e16c8dba5b51b312f. R54 dynamic ELF loader CI 46045709588 succeeded. R54 direct boot run 46047662573 observed 32/36/64 MiB success; 24 MiB failed at shell after kernel boot with "failed to reclaim memory"; 28 MiB still in progress at observation.

32 MiB job 140670863462: realmem=33554432, physmem=25931776, boot avail=23728128, page size 4096. Free pages 931 -> 546 (2,236,416 bytes at final gate), wired 4747 -> 5143. EXODUS_ID_PASS, EXODUS_VM_PASS, EXODUS_A9_REDUCED_PASS. Genuine shell, UFS root, read/hash /bin/sh and child-process execution, not multi-user or physical Tegra proof.

R55 commit 39926145701a3addd1ce573f239a84f78806ea42 adds separate controlled QEMU workflow tegra-freebsd-a9-maxusers.yml: exact same ELF/root snapshot/addresses, 24 and 64 MiB, only kern.maxusers=8 added to FDT bootargs, explicit runtime readback and unchanged shell checks. Pinned upstream source sys/kern/subr_param.c at 96841ea08dcf confirms explicit override and default autosizing floor of 32; NPROC=20+16*maxusers, MAXFILES=40+32*maxusers (further caps may apply). 64 MiB tuned case is a positive control; the untuned R54 matrix stays intact. R55 tuned result not yet observed. Do not claim 24 MiB support or savings until verified; lower process limits may be unsuitable for multi-user operation.

Evidence: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46047662573/job/140670863462
Source: https://github.com/freebsd/freebsd-src/blob/96841ea08dcf/sys/kern/subr_param.c
No physical Switch access; ADSP platform adaptation remains outstanding.
