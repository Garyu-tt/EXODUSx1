#include "handle_wait.h"
#include <assert.h>
#include <stdio.h>

static Result result;
static s32 selected;
static unsigned calls;
static uint64_t last_timeout;
static s32 last_count;
Result svcWaitSynchronization(s32 *index, const Handle *handles, s32 count, uint64_t timeout)
{
    s32 i;
    ++calls; last_timeout = timeout; last_count = count;
    for (i = 0; i < count; ++i) assert(handles[i] == (Handle)(i+1));
    *index = selected;
    return result;
}
int main(void)
{
    struct nx_handle_watch w[64]; struct nx_handle_result out;
    size_t i;
    for (i = 0; i < 64; ++i) w[i] = (struct nx_handle_watch){(Handle)(i+1), (uint32_t)(100+i)};
    assert(nx_handle_wait(NULL, 1, 0).status == NX_HANDLE_INVALID);
    assert(nx_handle_wait(w, 0, 0).status == NX_HANDLE_INVALID);
    assert(nx_handle_wait(w, 65, 0).status == NX_HANDLE_INVALID);
    assert(nx_handle_wait(w, 1, UINT64_C(1)<<63).status == NX_HANDLE_INVALID);
    w[1].token = w[0].token;
    assert(nx_handle_wait(w, 2, 0).status == NX_HANDLE_INVALID);
    w[1].token = 101; w[1].handle = 1;
    assert(nx_handle_wait(w, 2, 0).status == NX_HANDLE_INVALID);
    w[1].handle = 0;
    assert(nx_handle_wait(w, 2, 0).status == NX_HANDLE_INVALID);
    w[1].handle = 2; assert(calls == 0);
    selected = 63;
    out = nx_handle_wait(w, 64, NX_HANDLE_WAIT_FOREVER);
    assert(out.status == NX_HANDLE_READY && out.token == 163 && out.raw == 0);
    assert(last_count == 64 && last_timeout == UINT64_MAX);
    selected = -1;
    assert(nx_handle_wait(w, 1, 0).status == NX_HANDLE_ERROR);
    selected = 1;
    assert(nx_handle_wait(w, 1, 0).status == NX_HANDLE_ERROR);
    result = KERNELRESULT(TimedOut) | UINT32_C(0x80000000);
    out = nx_handle_wait(w, 1, 0);
    assert(out.status == NX_HANDLE_TIMEOUT && out.raw == result && last_timeout == 0);
    result = KERNELRESULT(Cancelled);
    out = nx_handle_wait(w, 1, 123);
    assert(out.status == NX_HANDLE_CANCELLED && out.raw == result && last_timeout == 123);
    result = KERNELRESULT(InvalidHandle);
    out = nx_handle_wait(w, 1, INT64_MAX);
    assert(out.status == NX_HANDLE_ERROR && out.raw == result && last_timeout == INT64_MAX);
    assert(calls == 6);
    puts("handle wait contract passed (mock kernel)");
    return 0;
}
