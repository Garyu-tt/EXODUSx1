# R73 — heartbeat loss and reliable log gates

R72 run 46056284376: 32 MiB PASS (job 140699449632), 64 MiB FAIL (job 140699449412). Both raw logs show native guest EXODUS_GUEST_HEARTBEAT_TIMEOUT, EXODUS_TIMEOUT_EXIT3 and subsequent EXODUS_GUEST_NATIVE_CODEC_PASS. 32 MiB additionally passed the independent whole-page oracle. 64 MiB failure occurs after successful guest exchange, with tr: write error: Broken pipe. Under pipefail, grep -q can exit before tr finishes. No 64 MiB whole-page success claim for this run.

R73 commit 82396b30c12c9e071acb8e80c9eb769e441588c9 normalizes UART output once to an artifact and runs exact match grep directly on that file. Retains every gate and final oracle; no changes to kernel, peer or heartbeat code. Retry pending. Target 32 MiB, 64 MiB control. R72 proves orderly disappearance of peer after ten exchanges and fail-closed timeout in emulation, followed by a fresh process/session; it does not prove recovery from hardware crash, noncoherent memory, or physical Tegra operation. No physical access.

R72: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46056284376
Retry not yet visible at observation.
