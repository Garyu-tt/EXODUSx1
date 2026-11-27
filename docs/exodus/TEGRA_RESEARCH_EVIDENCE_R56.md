# R56 — low-memory failure localization and minimal UFS kernel

R54 run 46047662573 completed: 32/36/64 MiB PASS, 24/28 MiB FAIL. At 28 MiB job 140670863408, shell reached and sysctl printed hw.realmem=29360128, hw.physmem=21737472, pagesize=4096; pid 13 sh was killed due to failed reclaim, then init panic. At 24 MiB shell killed earlier. This is a userspace memory-pressure failure, not failure to enter the kernel.

R55 run 46047875238: 64 MiB positive control PASS, 24 MiB FAIL at shell. At 64 MiB kern.maxusers=8, maxproc=148, actual maxfiles=1815 (not 296: upstream subr_param.c chooses max of formula and RAM-dependent term before cap). Final free pages 8603 vs 8599 baseline; wired 5138 vs 5144. Single observations, not statistically significant; no demonstrated meaningful savings.

R56 commit 43722405848031e3fc828e1a1259fd84adb7f07b builds EXODUS_A9_MIN independently from EXODUS_A9_REF, retaining local UFS, VirtIO, UART, VM, networking, MAC and Capsicum. Removes NFS_ROOT, NFSLOCKD, NFSCL, CD9660, PROCFS, SYSVSHM, SYSVMSG, SYSVSEM from inherited std.armv7, only for reference profile. Uses exact pinned source and prior UART correction; captures ELF SHA, llvm-size and PT_LOAD span against baseline 7028736 bytes. No changes to prior profile. Run 46048553785 in progress at observation; build and boot not yet proven. Next: if successful, pin new ELF hash, adapt guarded dynamic loader and run comparable 24/28/32/64 MiB shell tests. No physical hardware access or ADSP claim.

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46047875238
CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46048553785
