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
        const struct exodus_execution_target target={EXODUS_BACKEND_HORIZON,EXODUS_TARGET_TITLE_ID};
        return exodus_backend_route(target,canonical);
    }
    if (!strcmp(provider,"homebrew")) {
        char path[512];
        if (!exodus_nro_path(id,path)) return EXODUS_ROUTE_INVALID;
        /* Preserve the launch adapter's relative switch/ convention. */
        const char *target=!strncmp(path,"sdmc:/switch/",13) ? path+13 : path;
        snprintf(canonical,512,"%s",target);
        const struct exodus_execution_target execution={EXODUS_BACKEND_HORIZON,EXODUS_TARGET_NRO};
        return exodus_backend_route(execution,canonical);
    }
    return EXODUS_ROUTE_INVALID;
}
#endif
