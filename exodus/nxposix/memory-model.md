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
