# R15.3: kernel-owned virtual PID / exit metadata
AMP2 is intentionally a separate incompatible wire version; AMP1 remains
unchanged. A FreeBSD ARMv7 kernel module assigns virtual PID on the first
valid GETPID per remote task, keeps the mapping and records exit status
under the same mutex as sequence validation. A57 forwards the validated
positive 31-bit result unchanged. The host relay checks eight distinct
PIDs and zero exit replies; native kernel audit checks all eight exit
records with code 23. Reuses R14 monitor and R15.2 transport.
No real FreeBSD struct proc/pid namespace allocation yet, no wait4/fork,
no signals, no dynamic process creation, no physical ADSP. This is kernel
owned remote metadata, not actual POSIX process integration. Fresh CI
required. No new authority fallback. Refs #103 #105 #370 #371.

QA: distinct load markers for AMP1 prerequisite and AMP2 module. Require
exactly one of each; identical markers would incorrectly fail the final
serial audit even when both modules executed. No protocol behavior change.
