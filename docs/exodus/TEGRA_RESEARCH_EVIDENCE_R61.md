# R61 — actual guest shared memory and ping-pong

R60 run 46052571990 both 32 and 64 MiB PASS. Logs confirm 256-byte probe SHA256 40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880 and 1336-byte ELF SHA256 120cb0f9db9db2b20447c69f583938aaa4e0bc4ea137faa35c0012f02e30788c. Native mmap shared child write visible after wait4; private child write not visible; both process exits successful. 32 MiB baseline before fixture 824 free and 5111 wired pages. No physical or cross-ISA proof.

R61 adds tumor_freebsd_guest_pingpong.S (e487ef8417f55b2e7a9461c1e7c9d059bad99e3f), workflow 1a5b1a490e67d6b1af655f80ce655a47e4614f90. One 4096-byte shared page, distinct 64-byte spaced request and reply publication words, payload/complement validation, DMB ISH, monotonic 1..1000 request/reply tokens, bounded sched_yield polling, checked child status, unmap, final VM counters. 32 MiB fixed target; 64 MiB diagnostic control. Same exact R56 kernel and R60 binary-safe transfer. Compilation and runtime not yet confirmed at write. This is single vCPU coherent guest scheduling, not concurrent physical cores, noncoherent cache or TMR1 protocol validation. No device access.

CI R60: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46052571990
CI R61: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46052946417
