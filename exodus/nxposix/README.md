# NX-POSIX first event atom

Source inventory, pinned:
- libnx b156b4774fb2df7b93402169509266da7547fc4c: [socket.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/devices/socket.c) implements poll/select, socket operations, fcntl, ioctl and sysctl wrappers; actual individual operations may still fail on device. [newlib.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/newlib.c) implements libc thread/TLS/time hooks. [wait.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/kernel/wait.h) provides native handle/object waits; these are distinct from socket readiness.
- Autorun 1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877: [dlfcn shim](https://github.com/Garyu-tt/autorun/blob/1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877/horizon-wine/switch-shims/dlfcn.h) does not load libraries; [mman shim](https://github.com/Garyu-tt/autorun/blob/1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877/horizon-wine/box64-shims/sys/mman.h) explicitly documents the absent Linux mmap layer in the execution-only target. These are specific shim limitations, not proof that Horizon has no memory mapping facilities.

The first event atom is a small, stateless, bounded C99 adapter over poll. Up to 32 borrowed socket descriptors, level-triggered read/write readiness with explicit tokens; returns raw error/hangup flags and poll failures. No heap allocation, descriptor ownership, thread safety during descriptor mutation, wakeup/cancel mechanism, timers, native handle mixing, or kqueue ABI. Not an implementation of kevent. Existing Steam/Autorun code is not yet connected to it. No kernel changes. Invalid requests return EINVAL. Duplicate descriptors/tokens are rejected; output capacity must cover all watches. Single-threaded caller owns descriptor lifetime.

Host test (Linux):
    cc -std=c99 -Wall -Wextra -Werror -fsanitize=undefined reactor.c test_reactor.c -o test_reactor
    ./test_reactor

Executed successfully on Linux, covering readiness, level triggering, timeout, invalid inputs, hangup and invalid descriptor. The AF_UNIX host fixture is not a Switch portability assertion. Next use real TCP loopback on libnx with socketInitialize/socketExit, compare results, then add timed/wakeup events and explicit native-handle backend. Do not call it BSD binary compatibility until a separate ELF/ABI effort is validated.
