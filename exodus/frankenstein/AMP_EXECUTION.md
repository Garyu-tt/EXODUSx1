# AMP execution contract, stage 1

Reference-only byte protocol for a future local A57 executor and ADSP authority. Fixed 64-byte little-endian AMP1 frame, explicit generation/task/sequence, operation allowlist, one outstanding request, no shared pointers. The synthetic authority acknowledges GETPID (opaque task identity) and EXIT_INTENT (no actual process termination). These are NOT FreeBSD syscalls. Session errors close the contract; recovery requires new generation and externally quiesced transport. Existing TMR1 heartbeat is unchanged. No shared-memory integration, actual cross-process exchange, deadline handling, privilege authentication or physical memory mapping in this stage. QEMU binaries each independently execute the same fixture; not a two-kernel test.

Next gate: exchange these exact frames between separately compiled AArch64 and ARMv7 endpoints, then implement a supported local A57 exception/VM execution path and actual FreeBSD service authority. Preserve Horizon ownership and no hardware writes until board admission.

## Stage 2: separate peer processes

Host harness connects two separate QEMU user-mode processes with direct pipes; it does not generate replies. Both roles are tested on both ISAs, 128 identity requests and one exit-intent request per pair, crossing sequence 2^32 with full-width generation and task ID. A bounded external watchdog terminates stalled test peers. This verifies live byte exchange only, not a physical mailbox, hardware coherence, kernel exception handling or actual syscall execution. Production transport deadlines and authentication remain separate gates.
