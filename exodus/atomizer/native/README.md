# EX-014 native clock primitives

This is a candidate provider implementation, not a Win32 ABI or enabled dispatch path. Outputs are trusted host pointers; guest memory validation, BOOL/LastError translation and ABI thunks belong to a separate adapter. The caller must publish an immutable clock context with a monotonic tick source and stable frequency.

On Switch, ex_nx_clock() uses libnx armGetSystemTick and armGetSystemTickFreq. Its boot epoch is deliberately unknown: uptime queries fail without modifying output until a verified boot epoch is supplied. Do not treat process start as system boot. Counter regression/wrap and suspend semantics require device validation. QPC/QPF are paired in raw tick units; no Windows-specific resolution or epoch equivalence is asserted. Sleep is not implemented.

Host test:
    cd exodus/atomizer/native
    gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -O2 test_clock_atoms.c -o test-clock
    ./test-clock

Same command with clang also passed, with 16 assertions in each execution. Requires GCC/Clang unsigned 128-bit intermediate support. No target cross-build or physical Switch test performed.

References:
- https://switchbrew.github.io/libnx/counter_8h.html
- EX-014 candidate overlay and EX-016 NX-POSIX personality.

Next gate: device counter frequency, monotonicity across cores and sleep/resume, boot epoch evidence, guest pointer write/ABI tests, then differential Windows/Wine tests. Keep dispatch disabled until accepted.
