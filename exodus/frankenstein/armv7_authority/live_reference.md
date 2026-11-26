# R15.2: live two-system reference
Stacked on R15.1, whose native KLD execution is still pending when authored.
Boot genuine FreeBSD ARMv7 QEMU and require the native module's own test,
unload, then reload a fresh module generation. Native ARM32 relay validates
its caller PID and zero initial count. It only converts hex to raw bytes,
calls the fixed kernel sysctl and emits returned bytes. Host connects R14
A57 monitor UART to that console, starts A57 only after the relay is ready.
No qemu-arm synthetic authority process or fallback reply generator.
Require 16 native kernel replies, kernel counter 16, 8 A57 context results,
24 SVC, 16 HVC, 512 IRQ and module unload. Bounded host and monitor deadlines.
This is a mediated dual-QEMU transport, not physical shared memory or
Tegra ADSP, nor full remote FreeBSD process ownership. PID in R14 remains
fixture-local; native caller PID is a separate diagnostic. No target writes.
Refs #103 #105 #369 #370. Fresh CI required.
