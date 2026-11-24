# Hardware Atlas R03 — workload and scheduling research

Adds 15 candidate operations and 15 typed links to R01/R02, with 9 evidence sources. Donor source/documentation evidence is not permission or device acceptance. All operations remain non-admitted. No third-party implementation copied. See hardware-workloads.r03.json for source paths, blob hashes and acceptance targets.

## Donor findings

- deko3d exposes explicit Maxwell graphics queues and resource management; compile-time shader toolchain is a dependency.
- oss-nvjpg revision 45680e7 includes decoder channel open, submission and syncpoint/fence waiting. Kefir Hub pins this donor and carries a cache-lifetime patch. The reported defect was not reproduced here.
- Averne's FFmpeg NVTEGRA patch series of 2024-05-30 covers NVDEC/NVJPG/VIC and multiple decode codecs; upstream merge status unverified. The published performance comparison was on L4T, not a Switch benchmark. Dynamic frequency changes from the donor must not be automatically adopted.
- libnx hwopus has an IPC path with transfer memory and firmware-dependent command variants; the exact underlying execution unit has not been established.
- MSENC is a documented channel candidate only; no owned-frame encoder implementation or app permissions verified.

## Provider selection

hardware_scheduler.js is an independent pure JavaScript planning prototype. A request specifies an exact operation, profile, context, application-owned input and total estimated buffer bytes. Candidates must already be admitted, verified, safe, available and idle in the same context/profile; duplicated IDs are rejected. Specialized engines precede GPU then CPU, with deterministic tie-break. Unknown or limited thermal state selects admitted CPU only; critical state stops selection. Quarantine and free-memory budgets are checked. CPU fallback is not presumed equivalent and is subject to the same gates.

The 25 assertions passed in the authoring JavaScript environment. The repository Node CLI was not executed. Fixtures are synthetic and are not real provider registrations. Selection does not reserve resources, dispatch jobs, enforce security or read telemetry; production must add trusted registrations, synchronized reservation, cancellation and deadlines, failure quarantine, actual measurements and native acceptance. Estimated freeBytes must be defined consistently to avoid double-counting quarantined memory. The simple ranking is a policy default, not an observed performance optimum.

## Buffer contract

Allocate -> map -> submit -> fence complete -> unmap -> restore cache attributes -> release. A failed fence or uncertain cache state must quarantine memory with a bounded budget and stop admitting work when exhausted; no unsafe recycling. A concrete native lifetime implementation and fault-injection tests are required. No foreign process buffers, raw register writes, clock overrides, permission bypass or protected content.

## Next
Create a native C++ owned-buffer state machine and resource reservation contract with host fault-injection tests; then validate individual read-only or owned-workload providers under permitted application contexts. Actual console revision, firmware, temperature and throughput must be recorded. No device operations performed in R03.
