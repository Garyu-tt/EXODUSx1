# R74 — reciprocal heartbeat and development memory profiles

R73 retry 46057083086 both 32/64 MiB jobs passed. User removed 32 MiB target constraint; 32 MiB retained as historical verified minimal profile, 64 MiB default development and 128 MiB extended reference profile. No claim physical Tegra ADSP can use these budgets. No more low-memory optimization unless needed for hardware.

R74 adds guest 5f06667bea59fde76b45c8fcac53377757d0acfb and host e4a428ad468ecaf68cc50e27845a5975da215400, workflow d8ddda32b10636226d77e7dbe58afadc3ce40b5b. Retains actual pinned codec on both ISA and earlier forward timeout test, adds controlled responder exit after ten exchanges, host publishes eleventh and uses actual tmr_tick monotonic deadline, verifies previous ACK/reply and unchanged recovery state. Fresh session then exercises normal cooperative recovery and 1000-round regression. Runs at 64/128 MiB; runtime pending. This is orderly process loss, not physical crash, noncoherent cache, or hardware reset proof. No physical access.

R73: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46057083086
R74: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46057355774
