# R60 — binary-safe serial transfer

R59 run 46052205629: both 32 and 64 MiB guests passed baseline boot, writable snapshot and chunk acknowledgements, failed EXODUS_FIXTURE_PASS digest gate before ELF execution. No IPC execution result. Source inspection of pinned FreeBSD usr.bin/printf/printf.c reveals %b conversion runs escape(p,0,&len) then PF with %s, so embedded NUL truncates string output. This is a strong source-level explanation; prior guest file bytes were not captured. Format-string octal path instead uses explicit length and fwrite, and doubles escaped percent. R60 changes transfer to three-digit octal format literals and adds a 256-byte all-values probe with SHA256 before sending ELF. Logs actual guest size and digest. Preserves exact R56 kernel, 32 MiB target, 64 MiB control, single disposable root disk, and native mmap/fork fixture. CI commit db22d760bfdc948c87c7112cc80031420015ca1d, run 46052571990 in progress at observation. No physical access. Runtime success pending.

Source: https://github.com/freebsd/freebsd-src/blob/96841ea08dcf/usr.bin/printf/printf.c
CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46052571990
