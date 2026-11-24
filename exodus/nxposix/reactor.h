#ifndef EXODUS_REACTOR_H
#define EXODUS_REACTOR_H
#include <poll.h>
#include <stddef.h>
#include <stdint.h>

#define NX_REACTOR_MAX 32
struct nx_watch { int fd; short events; uint32_t token; };
struct nx_ready { uint32_t token; short events; };
/* Single-threaded, borrowed socket descriptors. No ownership or cancellation.
 * timeout_ms: -1 infinite, otherwise nonnegative. Returns ready count or -1.
 * Capacity must cover all watches; no readiness is silently discarded. */
int nx_reactor_wait(const struct nx_watch *watches, size_t count,
                    struct nx_ready *ready, size_t capacity, int timeout_ms);
#endif
