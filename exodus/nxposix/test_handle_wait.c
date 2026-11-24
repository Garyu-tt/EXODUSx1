#include "handle_wait.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>

static Result response;
static s32 selected;
static unsigned calls;
static u64 expected_ns;
static s32 expected_count;

Result waitHandles(s32 *idx, const Handle *handles, s32 count, u64 ns)
{
    ++calls;
    assert(count == expected_count && ns == expected_ns);
    for (s32 i = 0; i < count; ++i)
        assert(handles[i] == (Handle)(i + 10));
    *idx = selected;
    return response;
}

int main(void)
{
    struct nx_handle_watch w[NX_HANDLE_WAIT_MAX];
    for (size_t i = 0; i < NX_HANDLE_WAIT_MAX; ++i)
        w[i] = (struct nx_handle_watch){(Handle)(i + 10), (uint32_t)(i + 100)};
    uint32_t token = 999;
    Result raw = 999;
    expected_count = 2;
    expected_ns = 0;
    selected = 1;
    assert(nx_handle_wait(w, 2, &token, 0, &raw) == 1);
    assert(token == 101 && raw == 0 && calls == 1);
    assert(nx_handle_wait(w, 2, &token, 0, &raw) == 1); /* no implicit clear */
    expected_ns = UINT64_MAX;
    selected = 0;
    assert(nx_handle_wait(w, 2, &token, -1, &raw) == 1 && token == 100);
    expected_ns = (u64)INT_MAX * UINT64_C(1000000);
    expected_count = NX_HANDLE_WAIT_MAX;
    selected = NX_HANDLE_WAIT_MAX - 1;
    assert(nx_handle_wait(w, NX_HANDLE_WAIT_MAX, &token, INT_MAX, &raw) == 1);
    assert(token == 100 + NX_HANDLE_WAIT_MAX - 1);
    token = 999;
    expected_count = 2;
    expected_ns = UINT64_C(12000000);
    response = KERNELRESULT(TimedOut);
    assert(nx_handle_wait(w, 2, &token, 12, &raw) == 0 && token == 999 && raw == response);
    response = KERNELRESULT(InvalidHandle);
    assert(nx_handle_wait(w, 2, &token, 12, &raw) == -1 && errno == EBADF && raw == response && token == 999);
    response = 12345;
    assert(nx_handle_wait(w, 2, &token, 12, &raw) == -1 && errno == EIO && raw == response && token == 999);
    response = 0;
    selected = -1;
    assert(nx_handle_wait(w, 2, &token, 12, &raw) == -1 && errno == EIO && token == 999);
    selected = 2;
    assert(nx_handle_wait(w, 2, &token, 12, &raw) == -1 && errno == EIO && token == 999);

    unsigned before = calls;
    assert(nx_handle_wait(NULL, 2, &token, 0, &raw) == -1 && errno == EINVAL && raw == 0);
    assert(nx_handle_wait(w, 0, &token, 0, &raw) == -1 && errno == EINVAL);
    assert(nx_handle_wait(w, NX_HANDLE_WAIT_MAX + 1, &token, 0, &raw) == -1 && errno == EINVAL);
    assert(nx_handle_wait(w, 2, &token, -2, &raw) == -1 && errno == EINVAL);
    assert(nx_handle_wait(w, 2, NULL, 0, &raw) == -1 && errno == EINVAL);
    assert(nx_handle_wait(w, 2, &token, 0, NULL) == -1 && errno == EINVAL);
    w[1].handle = INVALID_HANDLE;
    assert(nx_handle_wait(w, 2, &token, 0, &raw) == -1 && errno == EBADF);
    w[1].handle = w[0].handle;
    assert(nx_handle_wait(w, 2, &token, 0, &raw) == -1 && errno == EINVAL);
    w[1].handle = 11;
    w[1].token = w[0].token;
    assert(nx_handle_wait(w, 2, &token, 0, &raw) == -1 && errno == EINVAL);
    assert(calls == before && token == 999);
    puts("handle wait contract tests passed (mock backend)");
    return 0;
}
