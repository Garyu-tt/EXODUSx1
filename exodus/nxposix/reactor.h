#ifndef EXODUS_REACTOR_H
#define EXODUS_REACTOR_H
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#define NX_REACTOR_MAX 32
struct nx_watch { int fd; short events; uint32_t token; };
struct nx_ready { uint32_t token; short events; };
/* Single-threaded, borrowed socket descriptors. No ownership or cancellation.
 * Supports POLLIN/POLLPRI/POLLOUT requests; poll error/hangup flags are returned.
 * Negative fds are ignored and duplicate fds are supported as in poll().
 * timeout_ms: -1 infinite, otherwise nonnegative. Returns ready watch count or -1.
 * capacity must cover all watches; EINTR is returned to the caller unchanged. */
int nx_reactor_wait(const struct nx_watch *watches, size_t count,
                    struct nx_ready *ready, size_t capacity, int timeout_ms);
#endif
