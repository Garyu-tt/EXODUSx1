#include "handle_wait.h"
#include <errno.h>

int nx_handle_wait(const struct nx_handle_watch *watches, size_t count,
                   uint32_t *token_out, int timeout_ms, Result *native_result)
{
    Handle handles[NX_HANDLE_WAIT_MAX];
    s32 idx = -1;
    Result rc;
    if (native_result) *native_result = 0;
    if (!watches || !token_out || !native_result || !count ||
        count > NX_HANDLE_WAIT_MAX || count > MAX_WAIT_OBJECTS ||
        timeout_ms < -1) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        if (watches[i].handle == INVALID_HANDLE) {
            errno = EBADF;
            return -1;
        }
        for (size_t j = 0; j < i; ++j) {
            if (watches[i].handle == watches[j].handle ||
                watches[i].token == watches[j].token) {
                errno = EINVAL;
                return -1;
            }
        }
        handles[i] = watches[i].handle;
    }
    const u64 ns = timeout_ms == -1 ? UINT64_MAX :
        (u64)timeout_ms * UINT64_C(1000000);
    rc = waitHandles(&idx, handles, (s32)count, ns);
    *native_result = rc;
    if (R_VALUE(rc) == KERNELRESULT(TimedOut)) return 0;
    if (R_FAILED(rc)) {
        errno = R_VALUE(rc) == KERNELRESULT(InvalidHandle) ? EBADF : EIO;
        return -1;
    }
    if (idx < 0 || (size_t)idx >= count) {
        errno = EIO;
        return -1;
    }
    *token_out = watches[idx].token;
    return 1;
}
