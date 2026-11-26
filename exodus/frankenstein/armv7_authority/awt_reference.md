# R15.7 native AWT1 admission
Stock ARMv7 FreeBSD module authenticates the single test parent by current
proc identity, and self-registering children by the native parent pointer
under proctree_lock. process_exit creates terminal AWT records; the actual
parent reaps its own children through waitpid, then checks AWT status.
Eight live polls, eight native exits (seven normal and one SIGTERM),
eight unauthorized child callers, 25 replay rejects. AWT consumes only
its own metadata, not the real kernel zombie. No remote A57 syscall yet.
No arbitrary PID or pointer input, no production namespace, no physical
ADSP. Fresh CI required. Refs #375 #373 #374.
