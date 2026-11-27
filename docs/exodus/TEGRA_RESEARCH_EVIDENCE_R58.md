# R58 — 32 MiB target freeze and native guest shared memory

User selected 32 MiB as target. No further RAM-minimization revisions. Existing R56/R57 allowed to complete for evidence; use already passing R53 kernel (exact digest in R54) for new functional tests. 64 MiB is diagnostic control only.

R58 adds tumor_freebsd_guest_shm.S (152ca8c01c4040580e2b542de32961288e3e7545) and guest workflow (ca99b7e040ffab1a66b7a48b737e06627bfe415c). Freestanding ARM EABI executable, FreeBSD brand note, direct mmap/fork/wait4/munmap/write/exit. Shared anonymous page must retain child payload and publication word after successful wait; private mapping negative control must remain zero. DMB instructions do not establish ADSP cache coherence. Local equivalent fixture compiled successfully with clang/lld as ELF32 ARM FreeBSD EABI5 (1408 bytes before final redundant reload); runtime not performed locally. CI attaches read-only disposable FAT fixture disk to reference QEMU A9 with existing root snapshot, requires baseline shell gates, guest success marker and exit status, runs at 32 and 64 MiB. CI run 46049090835 queued at observation. Additional device/mount/exec overhead may cause 32 MiB failure; diagnose separately. This is intra-guest process IPC, not actual TMR1 transfer or ARM64/ADSP communication. No physical access. R56 build and R57 admission still in progress at observation.

Source ABI evidence: https://github.com/freebsd/freebsd-src/blob/96841ea08dcf/sys/arm/arm/syscall.c ; https://github.com/freebsd/freebsd-src/blob/96841ea08dcf/sys/arm/arm/vm_machdep.c ; https://github.com/freebsd/freebsd-src/blob/96841ea08dcf/sys/sys/syscall.h ; https://github.com/freebsd/freebsd-src/blob/96841ea08dcf/sys/sys/mman.h

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46049090835
