# R69 — persistent TMR1 responder recovery

R68 run 46054729606 completed successfully at both 32 and 64 MiB. Seven fault cases rejected without ACK/reply, clean 1000-round rollover regression passed.

R69 adds native guest f5f9d747fc0e3d2be1ccad1f937d3fdd32283af6, ARM64 peer 2f2d38dae93d2d292ca71851f3ae4d43edb782bc and workflow 1c85e71059d8215c4b215f3d3f30041d0995b33c. Ten successful round trips, corrupt generation on eleventh, guest quiesces and publishes state=1, host checks previous ACK and reply unchanged, resets only host-owned fields then publishes command=1. Guest resets own ACK/reply, increments generation and publishes state=2, resumes same process for 1000 round trips crossing token wrap. Independent 4096-byte final page check includes control words and new generations. Bounded waits. This is one cooperative recovery in coherent emulation, not arbitrary peer crash, hardware reset, production codec integration or noncoherent cache proof. Fixed 32 MiB backing and 64 MiB control; no device access. Runtime pending.

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46055022987
