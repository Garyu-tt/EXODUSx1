#ifndef EXODUS_HANDLE_WAIT_H
#define EXODUS_HANDLE_WAIT_H
#include <switch.h>
#include <stddef.h>
#include <stdint.h>

#define NX_HANDLE_WAIT_MAX 64
#define NX_HANDLE_WAIT_FOREVER UINT64_MAX
struct nx_handle_watch { Handle handle; uint32_t token; };
enum nx_handle_status { NX_HANDLE_READY, NX_HANDLE_TIMEOUT, NX_HANDLE_CANCELLED,
                        NX_HANDLE_INVALID, NX_HANDLE_ERROR };
struct nx_handle_result {
    enum nx_handle_status status;
    uint32_t token; /* valid only for READY */
    Result raw; /* original kernel result, zero if no kernel error */
};
/* Borrowed handles; unique handles and tokens. Wait-any, no reset or close.
 * timeout_ns: 0 polls, INT64_MAX or less is finite, UINT64_MAX is infinite.
 * Raw syscall is intentional: cancellation is observable, not retried.
 * Callers must keep handles alive and unchanged throughout the wait. */
struct nx_handle_result nx_handle_wait(const struct nx_handle_watch *watches,
                                       size_t count, uint64_t timeout_ns);
#endif
