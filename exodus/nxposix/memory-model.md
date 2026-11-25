# NX memory and JIT capability model

Pinned reference: libnx b156b4774fb2df7b93402169509266da7547fc4c.

## Verified platform facts

- `virtmemFindAslr(size, guard)` selects a free virtual-address range while the libnx virtual-memory mutex is held. It does not allocate or map backing pages. `virtmemAddReservation` is libnx's own range bookkeeping.
- `svcSetHeapSize` expands/shrinks the process heap in 2 MiB increments and consumes the process memory resource limit.
- `svcSetMemoryPermission` cannot grant execute permission; libnx documents only transitions among no-access, read-only and read/write. It also rejects write-only.
- `svcMapMemory` is an alias mapping primitive, not general anonymous mmap. It reprotects the source range to no-access; on HOS 2.0+ the documented destination is the Stack region.
- Shared-memory creation/mapping is privileged. libnx `shmemCreate` explicitly documents that ordinary applications cannot normally use it.
- libnx already provides `kernel/jit.h` and `jitCreate`, `jitTransitionToWritable`, `jitTransitionToExecutable`, `jitClose`. There are two paths:
  - `JitType_SetProcessMemoryPermission`: privileged process-code-memory SVCs 0x73/0x77/0x78 and an own-process handle.
  - `JitType_CodeMemory`: SVCs 0x4B/0x4C. libnx warns that on HOS 5.0+ this path needs a kernel patch because ControlCodeMemory rejects the owning process; the loader must only hint syscalls that are actually usable.
- The CodeMemory path creates separate writable and executable aliases; libnx flushes D-cache on the writable alias and invalidates I-cache on the executable alias during transition. This is the correct existing JIT abstraction to probe before writing our own.

Sources:
- [libnx virtmem.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/kernel/virtmem.h)
- [libnx virtmem.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/kernel/virtmem.c)
- [libnx jit.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/kernel/jit.h)
- [libnx jit.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/kernel/jit.c)
- [libnx shmem.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/kernel/shmem.h)
- [Horizon SVC memory documentation](https://switchbrew.org/wiki/SVC#SetMemoryPermission)

## EXODUS design consequences

Do not implement Linux `mmap(PROT_EXEC)` by calling `svcSetMemoryPermission`. It cannot create executable mappings. Do not treat `virtmemFindAslr` as a physical allocator. Do not treat `svcMapMemory` as generic mmap. Do not assume privileged shared-memory or JIT SVCs are available in every launch environment.

Split memory into providers:
1. Private RW arena/provider for ordinary data pages, with explicit allocation, alignment, ownership, quotas and cleanup. This does not yet promise arbitrary unmap, file mapping or execute.
2. Virtual reservation tracker, using address-space discovery plus explicit reservation metadata; reservation alone is not committed memory.
3. Dedicated JIT provider wrapping libnx `Jit`, with separate RW/RX addresses and W^X state transitions. JIT unavailable is a hard capability result, not a reason to silently fall back to RWX.
4. Later, shared-memory provider only when environment capabilities prove those privileged calls are authorized.

Windows `VirtualAlloc` reserve/commit and POSIX `mmap` must be modeled as state machines, not aliases. File-backed mappings, copy-on-write, fixed-address requests, guard pages, page protection and executable pages each need explicit conformance tests. A game-specific runtime can avoid unused features, but cannot claim general API compatibility.

## Acceptance gates
- Build and run the supplied JIT probe on a real Switch with the exact HOS/Atmosphere/4IFIR versions recorded.
- Record syscall-hint availability and libnx JIT type; do not infer from HOS version alone.
- Verify generated AArch64 function returns 42 through the RX alias, then JitClose succeeds.
- Repeat create/execute/close 100 times; no leaked handles, mappings or memory growth.
- Confirm no RWX mapping and that unsupported JIT returns a clear unavailable result.
- Keep a normal non-JIT path for data mappings and fail unsupported PROT_EXEC explicitly.
- No kernel patch or SVC permission changes are part of this PR.

The source was inspected; the target NRO has not been built or run in this environment. No device access or memory changes performed.

## Existing Autorun provider audit (pinned commit 1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877)

The runtime already has a substantial Horizon memory provider; do not duplicate it in EXODUS.

| Capability | Existing implementation | Important qualification |
|---|---|---|
| mmap / munmap / mprotect | `dlls/ntdll/unix/horizon.c`, declarations in `horizon_mman.h` | Wine/NTDLL-specific provider, not a reusable POSIX library yet |
| Anonymous PROT_NONE | Reservation nodes in the mapping tree | Reserves address space, does not allocate backing pages |
| Large anonymous mappings | Demand-backed reservation from 256 MiB; faults commit chunks | Lazy fault handler and kernel block budget are part of the contract |
| File-backed mappings | Backing pages populated from fd | Ordinary path copies data into backing; it is not a kernel file mapping |
| MAP_SHARED file writes | Duplicated fd and writeback when final backing reference is released | Writeback semantics, not coherent cross-process shared pages |
| Lazy read-only file view | Optional for files >=1 MiB, read-only/non-executable, in 512 KiB chunks | Opt-in; private copy on demand; never writes through to the file |
| Shared section views | `horizon_mmap_section` and `horizon_memfile_map` | True alias needs code-memory syscalls plus section syscalls 0x74/0x75; otherwise runtime explicitly falls back to copies |
| Executable memory | `svcMapProcessCodeMemory` + `svcSetProcessMemoryPermission` | Requires syscall hints 0x73/0x77/0x78 and own process handle |
| W^X | `get_effective_horizon_prot` strips EXEC from simultaneous WRITE+EXEC | No RWX; executable transitions use cache flush/invalidate |
| Fixed mappings | MAP_FIXED, MAP_FIXED_NOREPLACE and try-fixed fallback | Must preserve replacement/rollback semantics |
| madvise | `horizon_madvise` | Currently logs and returns success without applying advice: a known semantic gap |

The default file-backed path copies bytes into Horizon backing pages. For writable MAP_SHARED mappings, the duplicated fd is written back on final backing release. Section-backed mappings are different: true shared aliases are only available when the loader grants both code-memory and section mapping SVCs; otherwise the runtime warns that section views are copies.

### Existing Box64 JIT provider

- `dlls/ntdll/unix/horizon_code_memory.h` and `horizon.c` provide a custom code-memory mapper.
- `wine_nx_code_memory_map` creates one CodeMemory object and maps a writable owner alias plus an executable slave alias in separate address ranges. It uses the process code-memory SVCs, not ordinary `svcSetMemoryPermission`.
- `horizon-wine/source/box64_code_arena.h` reuses and coalesces translated-code allocations inside those aliases. The dynarec documents the system-wide limit of ten kernel CodeMemory objects; per-function objects are not viable.
- `wow64_box64_dynarec.c` uses the custom Horizon mapper when available and a libnx `jitCreate` CodeMemory fallback otherwise.
- This is why the generic JIT probe in this PR is a capability diagnostic, not a replacement for the existing Box64 arena/provider.

### Known provider gaps to carry into the capability graph

1. `madvise` is a success-returning no-op. Consumers expecting DONTNEED reclamation or WILLNEED prefetch receive no effect.
2. Ordinary file-backed mappings are copy-backed; MAP_SHARED visibility is writeback-at-final-release rather than live coherent sharing.
3. Section views degrade from aliases to copies when 0x74/0x75 are not granted.
4. JIT depends on privileged loader hints and a finite global CodeMemory object budget.
5. The current Box64 code arena relies on guest memory lifecycle notifications to invalidate translations; writes not followed by the expected flush/protection/unmap notification can leave stale translated code.

EXODUS should wrap these existing providers as explicit capability nodes, preserve their limitations in provider metadata, and only replace a provider after differential tests. The JIT reservation-allocation rollback is being addressed separately in [Autorun PR #3](https://github.com/Garyu-tt/autorun/pull/3).

The generic libnx JIT probe may report either `JitType_CodeMemory` or `JitType_SetProcessMemoryPermission`. The existing Box64 dynarec fallback explicitly rejects any type other than `JitType_CodeMemory` (`wow64_box64_dynarec.c`, `map_arena`). Therefore a successful generic JIT probe using the process-permission backend does **not** prove that the current Box64 fallback can run. Keep the two capability results separate in EXODUS.
