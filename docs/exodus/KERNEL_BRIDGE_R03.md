# Kernel Bridge R03: provider contract

Stacked on #31. Adds a versioned synchronous C provider contract and a libnx candidate adapter. Own PID/TID and per-process memory counters only. Capability bits are declarations, not evidence of permissions. Unsupported, malformed and native errors are distinct. Native result valid only if native_called; value valid only if EXK_OK. Zero is valid. No allocations or ownership transfer. Context is borrowed for the synchronous call; no stronger thread safety than the backend. In-process source contract, not a cross-process or shared-library binary ABI.

R02 remains unchanged; migration to this interface and deduplication with existing wait/clock/memory PRs are subsequent tasks. No generic arbitrary syscall gateway.

Host test: `cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=undefined tools/kernel_bridge/provider_test.c -o /tmp/exk-test && /tmp/exk-test`. CI is configured for GCC and Clang. Native adapter requires devkitA64/libnx and device verification; no results claimed at submission.

R04: explicit authorized process lifecycle provider, opaque handles, ownership, bounded wait and two independent native application smoke test. Do not assume arbitrary spawn or fork.
