# R66 — concurrent cross-guest RAM exchange

R65 run 46053936814: memory (64) completed success; memory (32) completed success.

R66 adds ARMv7 FreeBSD responder 76109a42f3343b2a7ea05a202a4ab8235f7034e1, AArch64 requester df1cf488f924ca6d3a2cfbe25692cd1830a8ecf8 and workflow 863ab6c5f64599d00e3faa923197bcf839d59497. Reuses R65 exact kernel, 32 MiB RAM backing (4 KiB excluded from guest allocator), 64 MiB control, and verified serial fixture delivery. Host peer runs concurrently via Tcl pipeline with process timeout. One request/reply in flight, separate 32-bit publication and ACK, payload and complement, release/acquire plus guest DMB, 1000 round trips. Bounded guest poll and host deadline. Checks both process markers and final entire backing page. This is a simple stress protocol, not full TMR1 or physical cache-coherence evidence. QEMU-system FreeBSD and QEMU-user Linux ARM64 only. Compilation and runtime R66 pending. No physical device access.

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46054098187
