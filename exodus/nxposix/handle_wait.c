#include "handle_wait.h"

struct nx_handle_result nx_handle_wait(const struct nx_handle_watch *watches,
                                       size_t count, uint64_t timeout_ns)
{
    Handle handles[NX_HANDLE_WAIT_MAX];
    s32 index = -1;
    Result rc;
    size_t i, j;
    if (!watches || !count || count > NX_HANDLE_WAIT_MAX ||
        (timeout_ns > INT64_MAX && timeout_ns != NX_HANDLE_WAIT_FOREVER))
        return (struct nx_handle_result){NX_HANDLE_INVALID, 0, 0};
    for (i = 0; i < count; ++i) {
        if (!watches[i].handle)
            return (struct nx_handle_result){NX_HANDLE_INVALID, 0, 0};
        for (j = 0; j < i; ++j)
            if (watches[j].handle == watches[i].handle ||
                watches[j].token == watches[i].token)
                return (struct nx_handle_result){NX_HANDLE_INVALID, 0, 0};
        handles[i] = watches[i].handle;
    }
    rc = svcWaitSynchronization(&index, handles, (s32)count, timeout_ns);
    if (R_SUCCEEDED(rc)) {
        if (index < 0 || (size_t)index >= count)
            return (struct nx_handle_result){NX_HANDLE_ERROR, 0, rc};
        return (struct nx_handle_result){NX_HANDLE_READY, watches[index].token, rc};
    }
    if (R_VALUE(rc) == KERNELRESULT(TimedOut))
        return (struct nx_handle_result){NX_HANDLE_TIMEOUT, 0, rc};
    if (R_VALUE(rc) == KERNELRESULT(Cancelled))
        return (struct nx_handle_result){NX_HANDLE_CANCELLED, 0, rc};
    return (struct nx_handle_result){NX_HANDLE_ERROR, 0, rc};
}
