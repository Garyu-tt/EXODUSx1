# R67 — concurrent guest exchange PASS, TMR1 wire rollover

R66 run 46054098187 completed successfully at 32 and 64 MiB. Raw guest logs confirm 1000 request/reply rounds and successful exit in both jobs, CI also requires ARM64 peer marker and full backing page equality. 32 MiB guest hw.realmem=33550336 with reserved 4 KiB. This proves concurrent full-system QEMU FreeBSD / QEMU-user ARM64 coherent host exchange only.

R67 adds ARMv7 responder 3af71fb9435d9e045db6248f4fda1970424ee542, AArch64 peer 9b4cadda34fc1137c3befb435bc1a973b2fd8ee2 and workflow a38295fedcceb71e74f0f6db4cfd5a069eeddeec. Same shared page, fixed 32 MiB backing and 64 MiB control. 1000 request/reply rounds with explicit 24-byte TMR1-compatible frames, big-endian generation and sequence, initial sequence UINT32_MAX-500, uint32 publication and ACK rollover, independent final page oracle. Guest rejects wrong magic, version, role, reserved bytes, generation or sequence before reply. Test reimplements format and does not link production codec or prove fault injection yet. CI pending at observation, no hardware or second full guest kernel claim.

R66: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46054098187
R67: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46054516846
