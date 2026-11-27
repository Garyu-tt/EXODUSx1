# R70 — native Tumor codec in ARM64 peer

R69 run 46055022987 both 32 and 64 MiB jobs completed successfully. Cooperative recovery after ten good round trips and malformed eleventh request, same processes resume 1000 round trips, new generation and whole shared page checks passed. No evidence of physical reset or noncoherent memory.

R70 peer c9548511976014d77ca893e8d97fc49f2b88139c and workflow c04b0963344185836ee811e00851a0bd16221b5d fetch exact donor commit d6b9524ddba5aa429835934db4da685f45ddfaf4, verify exodus/frankenstein/ipc_native.c blob f0d043a8a590954a25104b87a808a301f29aaf42, build/run its selftests as AArch64 and replace independent host frame code with actual tmr_encode, tmr_init and tmr_receive using monotonic time. Sequence preset near rollover is fixture-only. Same ARM32 assembly receiver and R69 recovery protocol, fixed 32 MiB backing and 64 MiB control. ARM32 native C codec integration remains outstanding. Not production driver, physical ADSP, second full kernel, or authenticated transport. No device access. R70 compilation and runtime pending.

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46055210225
