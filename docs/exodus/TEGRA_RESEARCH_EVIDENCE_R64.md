# R64 — cross ISA PASS and actual FreeBSD guest shared RAM

R63 run 46053397114 job 140690007269 PASS: ELF32 ARM and ELF64 AArch64; both role assignments 1000 round trips each (4000 messages), both lanes cross uint32 wrap, stale generation rejected without ACK. Linux QEMU-user only.

R64 adds guest ARMv7 FreeBSD ELF fixture 7ce60b933957d87a81f012c1882fc111caec93f7, AArch64 Linux peer 8c258c77d7c84afa41c03eeb6eac84525504ff8e and workflow 989da50616fa596cdee8e9a0e40121c3b4b4a2b2. QEMU-only file-backed main RAM, 32 MiB target and 64 MiB control, final 4 KiB page excluded from FDT memory range and shared with separate ARM64 emulated process. Guest maps only that page through /dev/mem in disposable reference system, reads marker/payload/complement, writes reply/complement and ACK; ARM64 verifies after guest exits. Source reference pinned FreeBSD sys/arm/arm/mem.c memmmap and sys/arm/conf/DEFAULTS device mem; no hardware address claims. Uses R60 serial solely for fixture delivery and control, not data exchange. No physical access, no production driver or noncoherent proof; host ARM64 runs Linux QEMU-user, not a second kernel. 32 MiB backing includes reserved page, guest FDT reports 4096 bytes less. R64 unverified until CI, potential QEMU memory backend/ABI/mapping compatibility.

CI R63: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46053397114
CI R64: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46053603088
