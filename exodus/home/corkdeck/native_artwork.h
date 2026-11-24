/* Read-only Switch application artwork. Uses original local title control data. */
#ifndef EXODUS_NATIVE_ARTWORK_H
#define EXODUS_NATIVE_ARTWORK_H
#include "launcher_image.h"
#include "launch_route.h"
#include <stddef.h>
#ifdef __SWITCH__
#include <switch.h>
static inline int exodus_title_artwork(const char *id, struct launcher_icon *icon)
{
    uint64_t application_id;
    if (!exodus_title_id(id, &application_id)) return 0;
    NsApplicationControlData *data = calloc(1, sizeof(*data));
    if (!data) return 0;
    int ok = 0;
    if (R_SUCCEEDED(nsInitialize())) {
        u64 size = 0;
        const size_t offset = offsetof(NsApplicationControlData, icon);
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
            application_id, data, sizeof(*data), &size);
        if (R_SUCCEEDED(rc) && size > offset && size <= sizeof(*data) &&
            size - offset <= sizeof(data->icon))
            ok = launcher_image_decode(data->icon, (size_t)(size - offset), icon);
        nsExit();
    }
    free(data);
    return ok;
}
#else
static inline int exodus_title_artwork(const char *id, struct launcher_icon *icon)
{
    (void)id;
    (void)icon;
    return 0;
}
#endif
#endif
