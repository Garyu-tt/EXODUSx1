# EX-016: NX-POSIX / BSD compatibility personality

Owner asks whether a BSD compatibility layer can be derived from Switch kernel facilities. Horizon is not FreeBSD; its sockets service incorporates a BSD-derived networking stack. Mesosphere is a Horizon kernel reimplementation, not a FreeBSD kernel. Do not introduce a second kernel as a prerequisite.

## Design
EX-014 atomic capability graph owns shared semantic contracts. Add a POSIX/BSD personality for source-portable native ARM64 applications; EX-014 Windows personality may share low-level providers when semantics match. Distinguish source compatibility, FreeBSD ELF binary ABI, Linux ELF binary ABI and Win32/NT ABI. None implies the others. Start with static linking and libnx/newlib; no privileged patches by default.

| API family | Candidate provider | Semantic gap |
|---|---|---|
| socket/connect/send/recv/poll | libnx sockets over bsd:u | fd ownership, options, errno and blocking |
| open/read/write/stat | fsdev/newlib, Horizon FS | permissions, symlinks, atomic rename, mount boundaries |
| pthread/mutex/cond | libnx/newlib, Horizon threads and arbitration | cancellation, TLS, robust mutexes |
| clock_gettime/nanosleep | existing libc, Horizon ticks and waits | clocks, monotonic conversion, interrupt semantics |
| mmap/munmap/mprotect | user-space VM adapter and permitted SVCs | address space, alignment, protection, COW, file mappings |
| kqueue/kevent | user-space reactor over socket poll and supported waitable handles | EV_CLEAR, filters, timers, wakeups |
| fork/exec/waitpid/signals | explicit process/loader research | process creation permissions, COW, signal semantics, child lifecycle |
| dlopen/dlsym and ELF TLS | explicit loader research | relocations, ABI, TLS, unload |
| ioctl/sysctl | capability-specific adapters | command layouts and permissions |

Providers are candidates, not implementation or conformance evidence. Unsupported operations must fail with documented errors, never return fake success. Keep separate errno translations for native libc, FreeBSD and Linux. Do not assume bsd:u supports privileged socket types. No global service resets, no unrestricted SVC permissions. Keep native Nintendo applications unchanged.

## Phases
R01 inventory existing libnx/newlib, Autorun host shim and Mesosphere SVCs with source references; identify missing symbols from real source-port builds.
R02 test native ARM64 C programs for time, files, sockets, threads and failure semantics. Reuse existing implementations rather than duplicate them.
R03 user-space descriptor and event reactor, with bounded resources, concurrent close and cancellation tests.
R04 memory mapping and loader experiments only after ABI, ownership and protection review.
R05 optional broker/sysmodule for missing authorized cross-process semantics; kernel changes last, only when measured need exists and with emuMMC rollback.
R06 separate FreeBSD ARM64 ELF ABI experiment: loader, syscall trap/dispatch, structures, errno, TLS and signals. Linux ABI and x86 translation separately scoped. Do not claim arbitrary FreeBSD binaries can run.

## Acceptance
Host differential fixtures plus native libnx build and real Switch evidence; repeat under sleep/wake and resource pressure. Verify errors and cleanup as well as happy paths. Test source port of one small CLI and one networking program; no requirement to run a full desktop or privileged daemon. Measure RAM and CPU overhead relative to direct libnx. Preserve license notices. No claim of a working personality before these gates.

References:
- https://switchbrew.org/wiki/Sockets_services
- https://switchbrew.org/wiki/SVC
- https://github.com/switchbrew/libnx
- https://github.com/Atmosphere-NX/Atmosphere
- https://github.com/Garyu-tt/EXODUSx1/pull/17
