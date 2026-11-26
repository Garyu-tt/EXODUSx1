# Kernel Bridge R05: service supervisor

Stacked on #33. Fixed-size manifest (16 entries), dependency masks, cycle and duplicate validation, one-shot jobs, daemon readiness notification with generation-tagged process token, nonblocking process supervision and bounded restart with monotonic backoff. A successful spawn is not readiness. Daemon readiness is explicit and must later be bound to authenticated IPC or a validated health check. Failed poll invalidates readiness and retains ownership. Dependency gates govern new launches, not shutdown of already running dependents. No shell expansion.

Single-threaded, zero-initialized supervisor, exclusive stable process-manager backend, immutable borrowed manifest and strings. Do not reset/copy while owning children. Caller supplies monotonic milliseconds and calls tick periodically. Restart allowance is lifetime total, not a rolling budget. No implicit termination, cascade stop, boot-time device integration, lost-child reconciliation, privilege isolation or shutdown yet. Services can wait indefinitely if dependencies fail; report states to future shell. Spawn callback may block according to backend. No Horizon launch provider is claimed.

Host mock scenario: reject cycles; VFS one-shot -> network daemon -> shell one-shot; require readiness; reject stale tokens; invalidate readiness on poll error; bounded restart; monotonic time rejection; check process-table cleanup. CI for GCC/Clang with UBSan. Execution not verified at submission.

Next: connect the real authorized native process provider, add explicit shutdown and dependency-failure propagation, then expose service status and terminal sessions. This is not yet a replacement for Horizon boot init.
