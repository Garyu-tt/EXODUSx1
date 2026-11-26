# R15.14 live native identity + A57 clone
Single-core isolated clone joined to AWS1 reserve/commit and AWT1 in
a second live stock FreeBSD ARMv7 system. Private fixture clone SVC4093
requests native proxy PID, copies 4KiB and resumes child with zero.
Child writes private page, reports exit intent 23 via SVC4092; bridge
checks a live WNOHANG response, harness releases/reaps native proxy.
Parent resumes with kernel PID and calls FreeBSD ABI wait4 (7) twice.
First receives native status 0x1700, second ECHILD, status unchanged.
Both sides verify register/stack/page isolation. Five SVC/HVC pairs,
four kernel-generated frames. No synthetic PID, but still a fixed one
slot proxy, eager copy, private clone/exit ABI, synchronous bounded EL2
transport, no FD/SIMD/COW/full fork or actual Tegra. Fresh CI required.
