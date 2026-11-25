# EXODUSx1: Horizon-native operating system

Status: proposed architecture; owner-directed product goal, implementation pending.

## Direction
EXODUS evolves beyond a Windows launcher into an independent ARM64 operating environment on Nintendo Switch. Retain the Horizon kernel ABI initially; use existing Horizon services as bootstrap providers and progressively replace user-space functionality wherever technically and legally feasible. Do not require a Linux or Android kernel. Mesosphère is an optional later compatible kernel backend, not a mandatory migration.

## Layering
1. Tegra X1 hardware, Horizon kernel, SVC and IPC.
2. Hardware and kernel adapters: libnx, graphics, input, audio, storage, power and network providers. Inventory privileged dependencies and licenses.
3. EXODUS system services: init/supervisor, service registry, process/thread manager, VFS, permissions, networking, compositor, input, audio, package/update manager and recovery.
4. Shared capability graph: versioned contracts, provider selection, explicit unsupported states, provenance, conformance tests and fallback. Integrate the Wine atomizer (#17) and NX-POSIX (#19); do not equate source compatibility with foreign binary ABI.
5. Native SDK and runtime, POSIX source porting, optional Linux/BSD binary personalities and Windows translation when validated.
6. Controller-first Gaming shell, desktop session, terminal, headless mode; preserve original console mode where feasible.

## Terminal and system administration
- Interactive shell and terminal with UTF-8, history, keyboard/controller input, scrollback and eventually PTY semantics.
- VFS-backed ls/cd/cat/cp/mkdir, service status, process listing and resource metrics; no fake PIDs or fabricated metrics.
- SSH client/server, DNS, networking and VPN adapters, package manager with signed manifests, dependency resolution and rollback.
- Explicit permission boundaries; do not expose a root network shell by default.
- Native executable format initially NRO or another proven loader ABI; ELF and PE guest support only after ABI and memory tests.

## Implementation order and acceptance
- [ ] Inventory Horizon SVC and user-space services, ownership, lifecycle, privilege, memory budgets; publish dependency graph and replacement candidates.
- [ ] Bootstrap EXODUS shell in existing application mode; recover cleanly to known-good HOME.
- [ ] Implement bounded service supervisor, VFS and process model with mock host tests, devkitA64 builds and real console smoke tests.
- [ ] Implement usable terminal and package installation from signed test manifests with rollback.
- [ ] Validate sockets, clocks, waits, TLS, shared memory and dynamic loading with explicit failure modes.
- [ ] Gradually substitute user-space providers one at a time; measure latency and RAM on real hardware and retain recovery path.
- [ ] Integrate native KDE desktop track, without blocking the gaming shell.
- [ ] Document what still requires original Horizon services and what can be replaced; never claim kernel-only operation before proving it.

## Constraints
Retail Switch hardware memory and service privileges are bounded. No assumption of fork, mmap, POSIX signals, Linux /proc or DRM/KMS. No production NAND modifications or irreversible firmware replacement in early stages. Keep test deployments reversible; use owner-approved hardware testing. Respect upstream licenses, maintain attribution and separate redistributable source from proprietary firmware.

## Related work
#18 native Exodus shell; #19 NX-POSIX/BSD personality; #17 Wine Atomizer; #23 memory/JIT probe; #26 Horizon handle wait. Existing PRs remain useful foundation work, not completed OS features.
