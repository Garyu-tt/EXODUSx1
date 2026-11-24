#ifndef EXODUS_HANDLE_WAIT_H
#define EXODUS_HANDLE_WAIT_H

#include <switch.h>
#include <stddef.h>
#include <stdint.h>

#define NX_HANDLE_WAIT_MAX 32

struct nx_handle_watch {
    Handle handle;
    uint32_t token;
};

/* Borrowed kernel handles only, not socket descriptors or user-mode Waitables.
 * Returns 1 for a signalled handle, 0 for timeout, -1 for failure.
 * timeout_ms: -1 infinite, otherwise nonnegative. Does not clear or close
 * handles. Duplicate handles/tokens are rejected. No concurrent close.
 * token_out is changed only on success; native_result preserves backend
 * errors (including timeout). On local validation failure native_result=0
 * and errno describes the error. waitHandles may retry cancellations. */
int nx_handle_wait(const struct nx_handle_watch *watches, size_t count,
                   uint32_t *token_out, int timeout_ms, Result *native_result);
#endif
