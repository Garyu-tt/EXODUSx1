# EXODUS NX memory probe

Requires devkitPro, devkitA64 and libnx:

    make -C exodus/nxposix/memory_probe

Run the NRO on emuMMC first. It prints:
- HOS version and own-process handle;
- whether the environment hints the two libnx JIT syscall groups;
- the MemoryInfo/PageInfo returned by svcQueryMemory for one aligned heap allocation;
- the MemoryInfo/PageInfo for a free ASLR candidate returned by virtmemFindAslr under the required libnx lock.

The ASLR candidate is NOT reserved or mapped. The probe does not call svcSetHeapSize, svcMapMemory, svcSetMemoryPermission, svcCreateSharedMemory, or any JIT operation. It only allocates and frees one ordinary heap page and queries address metadata. It is intended to collect evidence before designing an mmap provider.

Not built or run here: devkitPro and Switch hardware are unavailable in the current environment. Record exact HOS, Atmosphere, 4IFIR and loader versions with the output.
