# R15.17 live Stage-2 COW pool
Connect confirmed serialized cow_pool model to single-core EL2 COW
fixture. Pool allocates original frame, shares by refcount on clone,
splits on child write and returns actual backing PA to descriptor update.
Child exit invalidates its Stage-2 descriptor and flushes current VMID
before release. Parent sole-owner fault upgrades without copy. Final
EL2 check reallocates released frame and verifies zeroing. Exactly two
write faults, one copy, three SVC/HVC pairs. Fixed two-page arena and
single core; no SMP shootdown, generic allocator or physical Tegra.
CI required. Separate from R15.14 native PID integration.
