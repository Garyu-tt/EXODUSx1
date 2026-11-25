// SPDX-License-Identifier: GPL-2.0-only
/* Read-only resource monitor, no privileged service writes. */
#ifndef EXODUS_CONTROL_METRICS_H
#define EXODUS_CONTROL_METRICS_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "horizon_settings.h"

struct exodus_metrics {
    char memory[64], storage[64], soc[64], board[64];
};

static inline void exodus_format_capacity(char out[64], uint64_t used, uint64_t total)
{
    if (!total || used > total) {
        snprintf(out, 64, "Unavailable");
        return;
    }
    snprintf(out, 64, "%llu / %llu MiB", (unsigned long long)(used / 1048576),
             (unsigned long long)(total / 1048576));
}

#ifdef __SWITCH__
static inline void exodus_temperature(char out[64], TsLocation location)
{
    if (hosversionAtLeast(17, 0, 0)) {
        TsSession session;
        Result rc = tsOpenSession(&session, location == TsLocation_Internal ?
                                  TsDeviceCode_LocationInternal : TsDeviceCode_LocationExternal);
        if (R_SUCCEEDED(rc)) {
            float temp;
            rc = tsSessionGetTemperature(&session, &temp);
            tsSessionClose(&session);
            if (R_SUCCEEDED(rc)) {
                if (temp >= -40.0f && temp <= 150.0f)
                    snprintf(out, 64, "%.1f C", (double)temp);
                return;
            }
        }
        exodus_service_error(out, rc);
    } else {
        s32 temp;
        Result rc = tsGetTemperature(location, &temp);
        if (R_FAILED(rc)) exodus_service_error(out, rc);
        else if (temp >= -40 && temp <= 150) snprintf(out, 64, "%ld C", (long)temp);
    }
}
#endif

static inline void exodus_read_metrics(struct exodus_metrics *m)
{
    memset(m, 0, sizeof(*m));
    snprintf(m->memory, 64, "Unavailable");
    snprintf(m->storage, 64, "Unavailable");
    snprintf(m->soc, 64, "Unavailable");
    snprintf(m->board, 64, "Unavailable");
#ifdef __SWITCH__
    u64 used, total;
    Result rc = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    if (R_SUCCEEDED(rc)) rc = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    if (R_SUCCEEDED(rc)) exodus_format_capacity(m->memory, used, total);
    else exodus_service_error(m->memory, rc);
    FsFileSystem sd;
    rc = fsOpenSdCardFileSystem(&sd);
    if (R_SUCCEEDED(rc)) {
        s64 free_bytes, total_bytes;
        rc = fsFsGetTotalSpace(&sd, "/", &total_bytes);
        if (R_SUCCEEDED(rc)) rc = fsFsGetFreeSpace(&sd, "/", &free_bytes);
        fsFsClose(&sd);
        if (R_SUCCEEDED(rc)) {
            if (total_bytes > 0 && free_bytes >= 0 && free_bytes <= total_bytes)
                exodus_format_capacity(m->storage, (uint64_t)(total_bytes - free_bytes), (uint64_t)total_bytes);
        } else exodus_service_error(m->storage, rc);
    } else exodus_service_error(m->storage, rc);
    rc = tsInitialize();
    if (R_SUCCEEDED(rc)) {
        exodus_temperature(m->soc, TsLocation_External);
        exodus_temperature(m->board, TsLocation_Internal);
        tsExit();
    } else {
        exodus_service_error(m->soc, rc);
        exodus_service_error(m->board, rc);
    }
#endif
}
#endif
