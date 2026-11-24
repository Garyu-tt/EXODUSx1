// SPDX-License-Identifier: GPL-2.0-only
/* CorkDeck's pinned runtime.c consumes argv[1] as a native executable path.
 * Never invoke a shell. The fixed NRO path is from EXODUS PR #11. */
#ifndef EXODUS_RUNTIME_HANDOFF_H
#define EXODUS_RUNTIME_HANDOFF_H
#include "launch_route.h"
#include "../../runtime/autorun_probe.h"

#define EXODUS_RUNTIME_ARGS_CAP 512

static inline int exodus_windows_path(const char *id)
{
    if (!id || strncmp(id, "sdmc:/", 6)) return 0;
    size_t len = strlen(id);
    if (len <= 10 || len >= EXODUS_RUNTIME_ARGS_CAP - sizeof(EXODUS_AUTORUN_PATH) - 4)
        return 0;
    const char *ext = id + len - 4;
    if (ext[0] != '.' || (ext[1] != 'e' && ext[1] != 'E') ||
        (ext[2] != 'x' && ext[2] != 'X') || (ext[3] != 'e' && ext[3] != 'E')) return 0;
    const char *segment = id + 6;
    for (const char *p = segment; ; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '/' || c == 0) {
            size_t n = (size_t)(p - segment);
            if (!n || (n == 1 && *segment == '.') ||
                (n == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
            if (!c) break;
            segment = p + 1;
        } else if (c < 32 || c == 127 || c == '"' || c == '\\' || c == ':') return 0;
    }
    return 1;
}

static inline int exodus_steam_app_id(const char *id)
{
    if (!id || !*id || *id == '0' || strlen(id) > 10) return 0;
    uint64_t n = 0;
    for (; *id; ++id) {
        if (*id < '0' || *id > '9') return 0;
        n = n * 10 + (unsigned)(*id - '0');
    }
    return n > 0 && n <= UINT32_MAX;
}

static inline int exodus_runtime_args(enum exodus_route route, const char *id,
                                       char out[EXODUS_RUNTIME_ARGS_CAP])
{
    int n;
    out[0] = 0;
    if (route == EXODUS_ROUTE_WINDOWS && exodus_windows_path(id))
        n = snprintf(out, EXODUS_RUNTIME_ARGS_CAP, "%s \"%s\"", EXODUS_AUTORUN_PATH, id);
    else if (route == EXODUS_ROUTE_STEAM && exodus_steam_app_id(id))
        n = snprintf(out, EXODUS_RUNTIME_ARGS_CAP, "%s", EXODUS_AUTORUN_PATH);
    else return 0;
    return n > 0 && n < EXODUS_RUNTIME_ARGS_CAP;
}
#endif
