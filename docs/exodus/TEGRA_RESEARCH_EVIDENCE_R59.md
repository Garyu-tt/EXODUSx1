# R59 — fixed 32 MiB target, single-disk guest IPC

R56 build 46048553785 success, exact kernel SHA256 5aed8989d139f8448c93044cc750329e7c9166c3a8fd258371997de9c8a0255b. PT_LOAD span 6258688 bytes vs baseline 7028736, saving 770048 bytes. R57 run 46048811743 admission success; 28/32/64 MiB PASS, 24 FAIL. At 28 MiB last free pages=281, wired=4627. Keep 32 MiB target per user; no further RAM minimization.

R58 run 46049090835 failed both jobs before native fixture execution. 32 MiB timed out after second VirtIO block attachment (16 MiB fixture enumerated vtbd0, 5 GiB root vtbd1); 64 MiB baseline shell passed but mounting vtbd1 as FAT failed EINVAL. Additional wired memory observed at 64 MiB (7785 pages after baseline vs 5144 in R54). No claim of guest IPC failure or success.

R59 workflow commit dc466c69323f311150a3fc30e94f8b12dcdb8a63 switches to exact successful minimal kernel digest and one original root disk (snapshot). Builds same ELF, sends 48-byte octal chunks over serial shell with per-chunk acknowledgements, verifies guest SHA256, executes after chmod. Mount writable only inside disposable snapshot; no real disk or device access. Tests at 32 and 64 MiB; 64 diagnostic control. CI run 46052205629 queued at observation. Runtime not yet verified; guest mount/transfer and ABI remain potential failure points. No physical ADSP claim.

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46052205629
