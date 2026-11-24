/* Provider routing, independent of the donor UI. Never executes shell commands. */
#ifndef EXODUS_LAUNCH_ROUTE_H
#define EXODUS_LAUNCH_ROUTE_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>

enum exodus_route { EXODUS_ROUTE_INVALID, EXODUS_ROUTE_SWITCH, EXODUS_ROUTE_HOMEBREW,
                    EXODUS_ROUTE_WINDOWS, EXODUS_ROUTE_STEAM, EXODUS_ROUTE_UNSUPPORTED };

static inline int exodus_title_id(const char *s, uint64_t *out)
{
    if (!s || strlen(s) != 16) return 0;
    uint64_t n = 0;
    for (int i = 0; i < 16; ++i) {
        unsigned char c = (unsigned char)s[i];
        unsigned v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return 0;
        n = (n << 4) | v;
    }
    if (!n) return 0;
    *out = n;
    return 1;
}

static inline int exodus_nro_path(const char *id, char out[512])
{
    if (!id) return 0;
    size_t len = strlen(id);
    if (len <= 4 || len >= 499 || id[0] == '/' || strcmp(id + len - 4, ".nro")) return 0;
    const char *segment = id;
    for (const char *p = id; ; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '/' || c == 0) {
            size_t size = (size_t)(p - segment);
            if (!size || (size == 1 && segment[0] == '.') ||
                (size == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
            if (!c) break;
            segment = p + 1;
        } else if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '"') return 0;
    }
    snprintf(out, 512, "sdmc:/switch/%s", id);
    return 1;
}

static inline enum exodus_route exodus_route_for(const char *provider, const char *id)
{
    if (!provider || !id || !*id) return EXODUS_ROUTE_INVALID;
    if (!strcmp(provider, "Switch")) {
        uint64_t n;
        return exodus_title_id(id, &n) ? EXODUS_ROUTE_SWITCH : EXODUS_ROUTE_INVALID;
    }
    if (!strcmp(provider, "Homebrew")) {
        char path[512];
        return exodus_nro_path(id, path) ? EXODUS_ROUTE_HOMEBREW : EXODUS_ROUTE_INVALID;
    }
    if (!strcmp(provider, "CorkDeck")) return EXODUS_ROUTE_WINDOWS;
    if (!strcmp(provider, "Steam")) return EXODUS_ROUTE_STEAM;
    return EXODUS_ROUTE_UNSUPPORTED;
}

static inline const char *exodus_route_name(enum exodus_route route)
{
    switch (route) {
        case EXODUS_ROUTE_SWITCH: return "Horizon application";
        case EXODUS_ROUTE_HOMEBREW: return "Homebrew loader";
        case EXODUS_ROUTE_WINDOWS: return "CorkDeck runtime (not connected)";
        case EXODUS_ROUTE_STEAM: return "Steam runtime (not connected)";
        case EXODUS_ROUTE_INVALID: return "Invalid target";
        default: return "Unsupported provider";
    }
}
#endif
