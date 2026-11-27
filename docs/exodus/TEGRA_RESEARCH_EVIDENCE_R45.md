# R45 — Static UART devmap hypothesis

R44 trace run 46036500312 job 140633131699 passed diagnostic gate. At c0627ef0 (LDRH r7,[r3,#16]) R0=0, R3=0x100; fault address 0x110. Caller _bt_fill uses vmem_bt_zone (subr_vmem.c); vm_mem_init normally initializes UMA then vmem_startup. This establishes null zone argument, not its upstream cause.

FreeBSD releng/15.1 sys/arm/qemu/virt_machdep.c registers gem5 for arm,vexpress and static UART devmap 0x1c090000, whereas QEMU vexpress-a9 UART is 0x10009000. initarm invokes platform_devmap_init/devmap_bootstrap then cninit before normal VM allocator startup. generic_bs_map calls pmap_mapdev, whose fallback may allocate KVA. Hypothesis: console lookup outside static mapping leads to premature vmem allocation. Not yet proven by call chain.

R45 commits bc5907a, f001ef2 and ed9d2ad add guarded experiment against exact R34 ELF hash. Only if unique expected MOVT encoding within gem5_devmap_init is present, change mapping base from 0x1c090000 to 0x10000000 in disposable ELF/raw image; otherwise fail closed. Covers vexpress-a9 UART, not real ADSP. Preserve before/after disassembly and boot log; kernel banner is gate. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46037440830. No production patch or physical hardware access.
