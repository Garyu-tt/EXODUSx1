# NX-POSIX first event atom

Source inventory, pinned:
- libnx b156b4774fb2df7b93402169509266da7547fc4c: [socket.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/devices/socket.c) maps public descriptors to BSD descriptors and delegates `poll()` to `bsdPoll()`; it also exposes socket, select, fcntl, ioctl and sysctl wrappers. The public `poll` wrapper currently rejects a NULL `fds` pointer even when `nfds == 0`, so this atom deliberately requires at least one watch. Individual operations still need device validation. [newlib.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/newlib.c) implements libc thread/TLS/time hooks. [wait.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/kernel/wait.h) provides native handle/object waits; these are distinct from socket readiness.
- Autorun 1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877: [dlfcn shim](https://github.com/Garyu-tt/autorun/blob/1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877/horizon-wine/switch-shims/dlfcn.h) does not load libraries; [mman shim](https://github.com/Garyu-tt/autorun/blob/1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877/horizon-wine/box64-shims/sys/mman.h) explicitly documents the absent Linux mmap layer in the execution-only target. These are specific shim limitations, not proof that Horizon has no memory mapping facilities.

The first event atom is a small, stateless, bounded C99 adapter over `poll()`. Up to 32 borrowed descriptors; level-triggered readiness with explicit tokens; returns raw error/hangup flags and backend failures. It supports `POLLIN`, `POLLPRI` and `POLLOUT` request masks. Negative descriptors are ignored, duplicate descriptors are supported (each watch gets its own token/result), and zero requested events are valid to observe error/hangup. Tokens must be unique. No heap allocation, descriptor ownership, thread safety during descriptor mutation, wakeup/cancel mechanism, timers, native handle mixing, or kqueue ABI. Not an implementation of `kevent`. Existing Steam/Autorun code is not yet connected to it. No kernel changes. Unsupported request flags and invalid arguments return `EINVAL`; `EINTR` is propagated without retry. Capacity must cover all watches.

## Host tests

    make -C exodus/nxposix test

Or:

    cc -std=c99 -Wall -Wextra -Werror -fsanitize=undefined reactor.c test_reactor.c -o test_reactor
    ./test_reactor

Host test covers read/write readiness, level triggering, duplicate descriptors, negative descriptors, zero-event watches, priority-event mask acceptance, timeout, invalid arguments, hangup, invalid descriptor and signal interruption. GCC and Clang host checks run in GitHub Actions. Host AF_UNIX fixtures do not prove Switch portability or BSD service behavior. Next device gate: compile a small libnx application with `socketInitialize`/BSD loopback, compare readiness and error behavior on real HOS, then add a separate Horizon handle-wait backend. Do not call this BSD binary compatibility until a separate ELF/ABI effort is validated.

## Next
- Add a libnx-native TCP loopback smoke test and compile with devkitA64.
- Add a separate adapter for Horizon waitable handles; do not mix handle IDs and socket FDs in one namespace.
- Design timer/wakeup/cancellation semantics before any kqueue-like interface.
- Memory mapping remains a separate investigation: libnx `virtmemFindAslr` only locates/reserves address ranges; SVC mapping/permissions and backing allocation need separate verified contracts.
