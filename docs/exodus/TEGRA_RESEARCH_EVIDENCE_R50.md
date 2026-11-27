# R50 — Confirmed compact userspace at 64 MiB; single-user memory floor

R49 run 46042463620: 64 MiB job 140653362786 and 96 MiB job 140653362423 passed root shell, VM counters, exact 4 MiB disk-backed file hash, 1 MiB tmpfs file and child process; EXODUS_A9_COMPACT_PASS. At 64 MiB final free pages 2069 (4096-byte pages), wired 6568. At 96 MiB final free pages 9988, wired 8182. 48 MiB job in_progress, no conclusion.

R50 commit dd3a89fcbeba75c9de7910c4def1dbf683ee43fc adds 32,40,48 MiB single-user matrix, using FreeBSD: -s bootargs (releng/15.1 sys/kern/subr_boot.c maps s to RB_SINGLE). Requires explicit init shell selection, prompt, identity and VM gates, reading/hash of /bin/sh and child process. No multi-user rc services or write workload in this test. Root image still verified and snapshot-backed; guarded QEMU-only UART patch retained. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46042857733. No physical Switch access or ADSP proof.
