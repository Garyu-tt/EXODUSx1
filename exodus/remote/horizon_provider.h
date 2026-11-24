#ifndef EXODUS_REMOTE_HORIZON_PROVIDER_H
#define EXODUS_REMOTE_HORIZON_PROVIDER_H
#include "remote_gate.h"
#include <inttypes.h>
#include <stdio.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

/* Read-only, synchronous. Caller owns service lifecycle serialization and
 * must supply an independently reviewed capability mask. Never open NAND. */
static inline exr_result exr_horizon_read(void *ctx, exr_op op, char *out, size_t cap) {
    (void)ctx;
    if (!out || !cap || cap > EXR_RESPONSE_CAP) return EXR_BAD_REQUEST;
    memset(out, 0, cap);
#ifdef __SWITCH__
    int n = -1;
    switch (op) {
    case EXR_STATUS: {
        u64 limit = 0, used = 0;
        if (R_FAILED(svcGetInfo(&limit, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0)) ||
            R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)) ||
            used > limit || !limit) return EXR_UNAVAILABLE;
        n = snprintf(out, cap, "scope=current_process\nlimit_bytes=%" PRIu64 "\nused_bytes=%" PRIu64 "\n", (uint64_t)limit, (uint64_t)used);
        break;
    }
    case EXR_BATTERY: {
        u32 percent = 0;
        if (R_FAILED(psmInitialize())) return EXR_UNAVAILABLE;
        Result rc = psmGetBatteryChargePercentage(&percent);
        psmExit();
        if (R_FAILED(rc) || percent > 100) return EXR_UNAVAILABLE;
        n = snprintf(out, cap, "charge_percent=%u\n", (unsigned)percent);
        break;
    }
    case EXR_STORAGE: {
        FsFileSystem sd;
        s64 total = 0, free_bytes = 0;
        if (R_FAILED(fsOpenSdCardFileSystem(&sd))) return EXR_UNAVAILABLE;
        Result rc = fsFsGetTotalSpace(&sd, "/", &total);
        if (R_SUCCEEDED(rc)) rc = fsFsGetFreeSpace(&sd, "/", &free_bytes);
        fsFsClose(&sd);
        if (R_FAILED(rc) || total <= 0 || free_bytes < 0 || free_bytes > total) return EXR_UNAVAILABLE;
        n = snprintf(out, cap, "scope=sd_card\ntotal_bytes=%" PRIu64 "\nfree_bytes=%" PRIu64 "\n", (uint64_t)total, (uint64_t)free_bytes);
        break;
    }
    default: return EXR_UNAVAILABLE;
    }
    if (n < 0 || (size_t)n >= cap) { memset(out, 0, cap); return EXR_PROVIDER_ERROR; }
    return EXR_OK;
#else
    (void)op;
    return EXR_UNAVAILABLE;
#endif
}

/* Probe only already-reviewed operations; no host hardware impersonation.
 * Refresh after resume or provider failure; no automatic background tasks. */
static inline exr_provider exr_horizon_probe(uint32_t reviewed) {
    exr_provider p = {0};
    p.reviewed = reviewed & EXR_KNOWN;
    p.read = exr_horizon_read;
    p.available = p.reviewed & EXR_BIT(EXR_DISCOVER);
    char out[EXR_RESPONSE_CAP];
    for (exr_op op = EXR_STATUS; op < EXR_COUNT; ++op) {
        if ((p.reviewed & EXR_BIT(op)) && exr_horizon_read(NULL, op, out, sizeof(out)) == EXR_OK)
            p.available |= EXR_BIT(op);
    }
    return p;
}
#endif
