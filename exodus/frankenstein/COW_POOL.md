# R15.16 COW page ownership and reuse model
Bounded caller-owned physical backing pool with mapping indices, per-frame
reference counts and exclusive-page no-copy path. Clone shares all mapped
pages without allocating. A split allocates/copies only if multiple
references exist; OOM rejects with no state mutation. Drop returns unused
frames to pool; allocation zeroes before exposure. Tests cover 3-way
sharing, pool exhaustion, isolation, exclusive promotion, and 256 reuse
cycles. Serialized model only. Not connected to Stage-2 hardware yet:
caller must invalidate translations and quiesce CPUs before releasing
frames. No SMP, physical allocator, actual reclaimed Tegra RAM or COW
hardware proof in this gate. CI on native sanitizer, ARMv7 and AArch64.
