/* Stable frontend launch descriptor. Syntax is not launch authorization. */
#ifndef EXODUS_NATIVE_TARGET_H
#define EXODUS_NATIVE_TARGET_H
#include "launch_route.h"

static inline enum exodus_route exodus_native_target(const char *provider, const char *id,
                                                       char canonical[512])
{
    if (!canonical) return EXODUS_ROUTE_INVALID;
    canonical[0]=0;
    if (!provider || !id) return EXODUS_ROUTE_INVALID;
    if (!strcmp(provider,"switch")) {
        uint64_t title;
        if (!exodus_title_id(id,&title)) return EXODUS_ROUTE_INVALID;
        snprintf(canonical,512,"%016llx",(unsigned long long)title);
        return EXODUS_ROUTE_SWITCH;
    }
    if (!strcmp(provider,"homebrew")) {
        char path[512];
        if (!exodus_nro_path(id,path)) return EXODUS_ROUTE_INVALID;
        /* The current native adapter accepts only relative switch/ IDs. */
        snprintf(canonical,512,"%s",id);
        return EXODUS_ROUTE_HOMEBREW;
    }
    return EXODUS_ROUTE_INVALID;
}
#endif
