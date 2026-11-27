# R68 — guest protocol rejection

R67 run 46054516846 both memory jobs successful. 32 MiB raw log confirms EXODUS_GUEST_TMR1_PASS, guest exit, all 4096 bytes equal independent expected page, hw.realmem 33550336. 1000 round trips over uint32 wrap with full 64-bit sequence. QEMU-system FreeBSD / Linux QEMU-user ARM64 only.

R68 adds fault peer 28ec089d8aaad0fe6b7979b3ab04db1a01db5ee4, guest 1646ecd95c1f9832fc116f2b822610ca3a99db07 and workflow f7302ae896aa44048f595eb7f96bea99b9995ae8. Seven independently initialized malformed first-request cases: magic, version, role, reserved, generation, sequence and publication token. Guest distinguishes validation rejection (exit 2 plus marker) from general failure (exit 1). ARM64 checks unchanged ACK and zero response. Test resets only after guest exits, then runs normal 1000-round rollover regression and independent full page oracle. This tests fresh process sessions and not live recovery, noncoherent cache or hardware fault behavior. 32 MiB target, 64 MiB control; no hardware access. R68 runtime pending at observation.

R67: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46054516846
R68: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46054729606
