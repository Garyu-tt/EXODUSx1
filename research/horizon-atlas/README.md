# Horizon Capability Atlas R01

This is an evidence inventory, not a working OS or a claim that every API can be invoked. Scope: retail Nintendo Switch 1 Erista/Mariko; exact installed HOS, execution context, NPDM and hardware variant must be recorded independently. No Switch 2 assumptions.

- `atlas.r01.json`: enumerated umbrella-header and SVC wrapper identifiers at the same pinned libnx revision used by EXODUS PR #49; historical Switchbrew service-family candidates are explicitly separate.
- `domain-graph.r01.json`: 20 capability domains, candidate dependencies, applications and constraints.
- `scan_libnx.py`: offline source scanner for all headers and source files, not limited to the umbrella. It emits syntactic candidates, not resolved C AST or an authoritative IPC table.

## Coverage and admission
The current public service registry is https://switchbrew.org/wiki/Services_API and the SVC registry is https://switchbrew.org/wiki/SVC. Direct wiki retrieval returned HTTP 403 in this session; the archived service list is historical and must not be treated as complete or current. Check the current registry and individual interface pages, including version bounds and Switch 1 versus Switch 2. Source declarations and inline/legacy wrappers are not unique kernel syscall numbers. No hardware testing occurred.

A production atom must have stable ID, source revision and location, syscall or IPC endpoint/interface/command, request and response types, min/max HOS, hardware variant, execution context, NPDM SVC/service ACL and FS permission, resource limits, side effects, dependencies, risk classification, provider, and evidence from host/native/device tests. Unknown or prohibited atoms fail closed. Do not probe restricted endpoints or change security state. No blanket privileged process, hardware, Nintendo online, or identity operations.

## Next layers
1. Parse all headers and source declarations at pinned libnx revision, then validate syntactic candidates with a real compiler AST.
2. Resolve public service endpoint/interface/command coverage against current Switchbrew and libnx source, distinguishing historical entries and gaps.
3. Record permitted context-specific read-only native probes, with expected failure and cleanup. Never promote a capability based on header existence.
4. Attach safe, reviewed atoms to Kernel Bridge #30–36, NX-POSIX #19, EXODUS HOME #50/#51, terminal #43, SSH #37 and Safe Mode #55.
5. Build POSIX, Win32 and desktop compatibility gap maps. Linux syscall ABI, fork/exec, D-Bus, Wayland, DRM/KMS, Vulkan, device drivers and arbitrary binary compatibility must not be assumed native.
