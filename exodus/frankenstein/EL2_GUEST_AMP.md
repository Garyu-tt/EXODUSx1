# R11: guest-originated AMP requests and wait/resume
Derived from R10 (7/7 PASS). Four A57 cores, eight pinned trusted EL1
fixtures. HVC #0x44 carries a reference opcode in x8 and scalar in x1,
returns result in saved x8 (not a FreeBSD or Horizon syscall ABI).
Save full R09 integer/SIMD/EL1 state on HVC as on timer IRQ. Each guest
submits GETPID then EXIT_INTENT, checks the returned values and records
completion in its private page. The monitor never ERETs into a REQUEST
lane: it runs the sibling if runnable, otherwise polls locally in EL2
with a bounded idle deadline; CPU0 owns the nonblocking UART pump.
No global lock; only owning CPU completes each session and updates its
saved result. Primary continues pumping during final rendezvous.
CI expects 16 real HVCs, 16 ARM32 responses, 512 timer IRQs, eight guest
completion flags and the prior memory/SIMD checks. EXIT_INTENT is only an
acknowledgement, not real process termination.

No EL0 SVC routing, actual FreeBSD syscall implementation, scheduler
sleep interrupt, guest-created mappings, physical ADSP or Horizon.
Local idle polling is a reference compromise, not production power
management; lane failure remains global fail-closed. CI required.
Refs #103 #105 #363.
