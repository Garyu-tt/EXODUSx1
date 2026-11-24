/* Device-only fixture using ordinary thread handles, not privileged events. */
#include "handle_wait.h"
#include <stdio.h>

static void finish(void *arg) { (void)arg; }

int nx_handle_smoke(void)
{
    Thread worker;
    Result raw = 0;
    uint32_t token = 0;
    struct nx_handle_watch w[2] = {{threadGetCurHandle(), 10}, {INVALID_HANDLE, 20}};
    int status = 1;
    if (nx_handle_wait(w, 1, &token, 0, &raw) != 0 || token != 0) {
        printf("self wait timeout failed: 0x%08x\n", (unsigned)raw);
        return 1;
    }
    raw = threadCreate(&worker, finish, NULL, NULL, 0x4000, 0x2c, -2);
    if (R_FAILED(raw)) {
        printf("threadCreate failed: 0x%08x\n", (unsigned)raw);
        return 1;
    }
    w[1].handle = worker.handle;
    raw = threadStart(&worker);
    if (R_SUCCEEDED(raw)) {
        if (nx_handle_wait(w, 2, &token, 2000, &raw) == 1 && token == 20 &&
            nx_handle_wait(w, 2, &token, 0, &raw) == 1 && token == 20)
            status = 0;
        else
            printf("worker handle wait failed: 0x%08x\n", (unsigned)raw);
        raw = threadWaitForExit(&worker);
        if (R_FAILED(raw)) {
            printf("threadWaitForExit failed: 0x%08x\n", (unsigned)raw);
            return 1; /* do not free a possibly live thread */
        }
    } else {
        printf("threadStart failed: 0x%08x\n", (unsigned)raw);
    }
    raw = threadClose(&worker);
    if (R_FAILED(raw)) {
        printf("threadClose failed: 0x%08x\n", (unsigned)raw);
        status = 1;
    }
    if (!status) puts("Horizon handle smoke passed");
    return status;
}
