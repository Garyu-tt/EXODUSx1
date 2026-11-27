# R71 — same Tumor codec on both architectures

R70 run 46055210225 completed successfully at 32 and 64 MiB. Pinned original ARM64 codec selftests and recovery exchange passed.

R71 adds freestanding ARMv7 adapter 5224799e5ed5cad638a32eafebc987a8ad98a693, assembly bridge 3f26a809ad01c7d812d2db685c4254023dc11f1f and workflow 4a210b083a10abdd41b4d715353869c39efa2ddb. Both peers include original ipc_native.c pinned to commit d6b9524ddba5aa429835934db4da685f45ddfaf4 and blob f0d043a8a590954a25104b87a808a301f29aaf42. Guest calls tmr_init, tmr_receive, tmr_encode with native FreeBSD clock_gettime syscall 232, CLOCK_MONOTONIC=4, ARM time_t 64-bit and 32-bit long verified against pinned FreeBSD source. Assembly retains mapping, token handling, publication barrier and R69 cooperative reset ownership. Guest runtime helpers support freestanding compilation. Same 10 successful rounds then corrupt request, recovery and 1000 rounds. Fixed 32 MiB backing and 64 MiB control. Runtime and compilation pending; original codec timeout tested at receipt, separate bounded polling remains. Not physical cache or hardware reset proof, no actual device access.

CI run not yet visible at observation.
