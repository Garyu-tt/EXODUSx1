# R72 — native heartbeat loss detection

R71 retry 46055593513 passed both 32 and 64 MiB. Raw 32 MiB log proves linked global tmr_encode/tmr_receive, codec selftest deadline and generation, guest native-codec PASS and full 4096-byte oracle. Same original codec on both architectures.

R72 guest adapter 18e6aeac10517060ccd622a814a44ad475680764 calls actual tmr_tick during idle polling with native monotonic clock. Assembly a8c967532384370f7d5681bf9ae1504e4a52f336 distinguishes heartbeat timeout exit 3 from general failure, no ACK or reset on silence. Host dbf5767f5c716605867c1f8f6fb473de09378a9a stops after ten valid exchanges and checks both tokens, both previous frames, and control words unchanged. Workflow 0d505bd252d3dc46b6f025a27db7bfc85fba3b7f executes silence then starts a fresh test process/session and repeats successful live recovery regression. Original codec deadline 3000 ms. Not automatic recovery from hardware crash; no physical cache or hardware tests. 32 MiB target and 64 MiB control. Runtime pending.

CI run not visible at observation.
