# R15.15 stage-2 Copy-on-Write
Independent branch from confirmed R15.13. Parent and child share one
RX page and initially one read-only physical data/stack page. Child
first store triggers EL0 stage-2 write permission abort; EL2 checks
ESR, FAR and HPFAR, copies 4KiB once, installs private RW mapping with
break-before-make and retries the store. Parent's later store triggers
a second fault and upgrades its now-exclusive original page without
copy. Explicit counters assert zero copies at clone, one at child exit,
one at final parent exit; physical page sentinels prove isolation.
Two faults, three SVC/HVC pairs expected. One reserved spare page,
single core, no generic refcount, allocator, SMP shootdown or native
AWS1 integration. Reserved memory is not yet physically reclaimed.
No Tegra proof. Fresh CI required.
