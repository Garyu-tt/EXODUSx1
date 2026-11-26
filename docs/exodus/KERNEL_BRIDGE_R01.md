# Kernel Bridge R01: desktop without a Linux kernel

Related: #28 (native OS), #29 (Qt/KDE), #19 (NX-POSIX), #23 (memory), #27 (wait), #17 (Wine capability graph).

Horizon kernel primitives are not the same as its console policy. Introduce versioned EXODUS contracts for scheduling, memory, handles, IPC, process lifecycle, VFS, graphics and input. Initial providers reuse licensed libnx and stock services; replace one provider at a time. No direct hardware or privileged SVC access is assumed. No patching or firmware installation in R01.

## Deliverable
The `tools/kernel_bridge/kernel_bridge.mjs` script contains an initial 27-node dependency graph and a fail-closed graph analyzer. It reports dependency order, residual Horizon user-space dependencies and unverified closure. `documented` means a candidate primitive/service exists in public references, NOT that it is accessible in our process or satisfies the contract. `planned` is not implemented; `device_verified` is reserved for later verified evidence. The graph is deliberately provisional, not an exhaustive kernel audit.

Run `node kernel_bridge.mjs --test`, then `node kernel_bridge.mjs exodus.kde` or `node kernel_bridge.mjs exodus.headless`. Ten pure JavaScript assertions were executed in a JS runtime before submission; Node CLI and device runs are pending.

## Next implementation gates
1. Pin libnx and Atmosphere revisions, record exact SVC/IPC contracts, firmware applicability, permissions, handle ownership and return codes. Sources: https://switchbrew.org/wiki/SVC ; https://switchbrew.org/wiki/Services_API ; https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h ; https://github.com/Atmosphere-NX/Atmosphere .
2. Read-only native admission probe: report firmware, execution mode, memory budget, own process identity, authorized service availability and actual errors. Do not infer privilege from API declarations. Do not mark any node verified without hardware logs.
3. Stable provider ABI, explicit unsupported/permission-denied errors, lifetime and resource budgets; reuse existing wait/clock/memory implementations rather than duplicate them.
4. Demonstrate independent process launch, exit and cleanup of two authorized native test applications; do not simulate PIDs or assume POSIX fork. Add supervisor and service registry.
5. Headless shell and terminal session, then native compositor proof with two surfaces, focus, input and resource limits. Desktop and gaming session switch must have recovery fallback.
6. Measure and remove bootstrap service dependencies individually. Preserve a compatible console session as a separate provider, not a dependency of native desktop applications. Kernel-only operation is an eventual target, not a present capability.

Graph dependency choices are initial hypotheses; source references establish API existence, not every proposed edge. No hardware tested and no firmware modified.
