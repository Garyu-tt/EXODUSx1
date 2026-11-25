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
    const char *prefix = "sdmc:/switch/";
    if (!strncmp(id, "sdmc:/Games/", 12)) {
        prefix = "sdmc:/Games/";
        id += 12;
    }
    size_t len = strlen(id);
    if (len <= 4 || len >= 512 - strlen(prefix) || id[0] == '/') return 0;
    const char *ext = id + len - 4;
    if (ext[0] != '.' || (ext[1] != 'n' && ext[1] != 'N') ||
        (ext[2] != 'r' && ext[2] != 'R') || (ext[3] != 'o' && ext[3] != 'O')) return 0;
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
    snprintf(out, 512, "%s%s", prefix, id);
    return 1;
}

/* Execution backend and target ABI are independent of presentation/provider. */
enum exodus_backend { EXODUS_BACKEND_NONE, EXODUS_BACKEND_HORIZON,
    EXODUS_BACKEND_BSD, EXODUS_BACKEND_CORKDECK, EXODUS_BACKEND_STEAM };
enum exodus_target_kind { EXODUS_TARGET_NONE, EXODUS_TARGET_TITLE_ID,
    EXODUS_TARGET_NRO, EXODUS_TARGET_ELF, EXODUS_TARGET_PE,
    EXODUS_TARGET_STEAM_LIBRARY, EXODUS_TARGET_STEAM_APP };
struct exodus_execution_target { enum exodus_backend backend; enum exodus_target_kind kind; };

static inline enum exodus_route exodus_backend_route(struct exodus_execution_target target, const char *id)
{
    if (!id || !*id) return EXODUS_ROUTE_INVALID;
    switch (target.backend) {
        case EXODUS_BACKEND_HORIZON:
            if (target.kind == EXODUS_TARGET_TITLE_ID) {
                uint64_t title;
                return exodus_title_id(id,&title) ? EXODUS_ROUTE_SWITCH : EXODUS_ROUTE_INVALID;
            }
            if (target.kind == EXODUS_TARGET_NRO) {
                char path[512];
                return exodus_nro_path(id,path) ? EXODUS_ROUTE_HOMEBREW : EXODUS_ROUTE_INVALID;
            }
            return EXODUS_ROUTE_INVALID;
        case EXODUS_BACKEND_CORKDECK:
            if (target.kind == EXODUS_TARGET_PE) return EXODUS_ROUTE_WINDOWS;
            if (target.kind == EXODUS_TARGET_STEAM_LIBRARY) return EXODUS_ROUTE_STEAM;
            return EXODUS_ROUTE_INVALID;
        case EXODUS_BACKEND_BSD:
            return target.kind == EXODUS_TARGET_ELF ? EXODUS_ROUTE_UNSUPPORTED : EXODUS_ROUTE_INVALID;
        case EXODUS_BACKEND_STEAM:
            return target.kind == EXODUS_TARGET_STEAM_APP ? EXODUS_ROUTE_UNSUPPORTED : EXODUS_ROUTE_INVALID;
        case EXODUS_BACKEND_NONE:
            return EXODUS_ROUTE_UNSUPPORTED;
        default: return EXODUS_ROUTE_INVALID;
    }
}
static inline const char *exodus_backend_name(enum exodus_backend backend)
{
    switch (backend) {
        case EXODUS_BACKEND_HORIZON: return "Horizon";
        case EXODUS_BACKEND_BSD: return "FreeBSD";
        case EXODUS_BACKEND_CORKDECK: return "CorkDeck";
        case EXODUS_BACKEND_STEAM: return "Steam";
        default: return "No backend";
    }
}
/* Legacy provider-string API: compatibility adapter only. */
static inline enum exodus_route exodus_route_for(const char *provider, const char *id)
{
    if (!provider || !id || !*id) return EXODUS_ROUTE_INVALID;
    struct exodus_execution_target target = {EXODUS_BACKEND_NONE, EXODUS_TARGET_NONE};
    if (!strcmp(provider,"Switch")) {
        target.backend=EXODUS_BACKEND_HORIZON; target.kind=EXODUS_TARGET_TITLE_ID;
    } else if (!strcmp(provider,"Homebrew")) {
        target.backend=EXODUS_BACKEND_HORIZON; target.kind=EXODUS_TARGET_NRO;
    } else if (!strcmp(provider,"CorkDeck")) {
        target.backend=EXODUS_BACKEND_CORKDECK; target.kind=EXODUS_TARGET_PE;
    } else if (!strcmp(provider,"Steam")) {
        target.backend=EXODUS_BACKEND_CORKDECK; target.kind=EXODUS_TARGET_STEAM_LIBRARY;
    }
    return exodus_backend_route(target,id);
}

static inline const char *exodus_route_name(enum exodus_route route)
{
    switch (route) {
        case EXODUS_ROUTE_SWITCH: return "Horizon application";
        case EXODUS_ROUTE_HOMEBREW: return "Homebrew loader";
        case EXODUS_ROUTE_WINDOWS: return "CorkDeck runtime";
        case EXODUS_ROUTE_STEAM: return "Open CorkDeck library (select Steam game there)";
        case EXODUS_ROUTE_INVALID: return "Invalid target";
        default: return "Unsupported provider";
    }
}
#endif
