#ifndef EXODUS_KERNEL_BRIDGE_HORIZON_H
#define EXODUS_KERNEL_BRIDGE_HORIZON_H
#include "kernel_bridge.h"
#ifdef __SWITCH__
#include <switch.h>
static inline uint32_t exk_horizon_query(void *ctx, unsigned field, uint64_t *out) {
    (void)ctx;
    switch (field) {
        case EXK_PID: return svcGetProcessId(out,CUR_PROCESS_HANDLE);
        case EXK_TID: return svcGetThreadId(out,CUR_THREAD_HANDLE);
        case EXK_MEMORY_LIMIT: return svcGetInfo(out,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
        case EXK_MEMORY_USED: return svcGetInfo(out,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
        default: return UINT32_MAX;
    }
}
static inline struct exk_provider exk_horizon_provider(void) {
    const struct exk_provider p = {EXK_ABI_V1,EXK_ALL_FIELDS,exk_horizon_query,NULL};
    return p;
}
#endif
#endif
