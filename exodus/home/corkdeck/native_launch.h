/* Experimental native launch adapter. Explicit confirmation required by caller. */
#ifndef EXODUS_NATIVE_LAUNCH_H
#define EXODUS_NATIVE_LAUNCH_H
#include "launch_route.h"
#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>

static inline int exodus_registered_title(uint64_t id)
{
    if (R_FAILED(nsInitialize())) return 0;
    NsApplicationRecord records[32];
    int found = 0;
    for (s32 offset = 0; offset < 4096 && !found; offset += 32) {
        s32 count = 0;
        Result rc = nsListApplicationRecord(records, 32, offset, &count);
        if (R_FAILED(rc) || count < 0 || count > 32) break;
        for (s32 i = 0; i < count; ++i)
            if (records[i].application_id == id) found = 1;
        if (count < 32) break;
    }
    nsExit();
    return found;
}
#endif

static inline int exodus_launch(enum exodus_route route, const char *id, char message[256])
{
#ifdef __SWITCH__
    Result rc;
    if (route == EXODUS_ROUTE_SWITCH) {
        uint64_t title;
        AppletType type = appletGetAppletType();
        if (!exodus_title_id(id, &title) || !exodus_registered_title(title)) {
            snprintf(message, 256, "Title not verified in local inventory.");
            return 0;
        }
        if (type != AppletType_Application && type != AppletType_SystemApplication &&
            !(type == AppletType_LibraryApplet && hosversionAtLeast(5, 0, 0))) {
            snprintf(message, 256, "Launch unavailable in current applet mode.");
            return 0;
        }
        rc = appletRequestLaunchApplication(title, NULL);
    } else if (route == EXODUS_ROUTE_HOMEBREW) {
        char path[512], args[516];
        struct stat st;
        unsigned char magic[4];
        if (!exodus_nro_path(id, path) || !envHasNextLoad() ||
            stat(path, &st) || !S_ISREG(st.st_mode)) {
            snprintf(message, 256, "NRO or loader unavailable.");
            return 0;
        }
        FILE *f = fopen(path, "rb");
        if (!f) { snprintf(message, 256, "Cannot open NRO."); return 0; }
        int ok = st.st_size >= 128 && !fseek(f, 16, SEEK_SET) &&
                 fread(magic, 1, 4, f) == 4 && !memcmp(magic, "NRO0", 4);
        fclose(f);
        if (!ok) { snprintf(message, 256, "Invalid NRO header."); return 0; }
        snprintf(args, sizeof(args), "\"%s\"", path);
        rc = envSetNextLoad(path, args);
    } else {
        snprintf(message, 256, "%s", exodus_route_name(route));
        return 0;
    }
    if (R_FAILED(rc)) {
        snprintf(message, 256, "Launch request failed: 0x%08lx", (unsigned long)rc);
        return 0;
    }
    return 1; /* caller closes UI normally; request accepted, not proof of execution */
#else
    (void)id;
    snprintf(message, 256, "%s: native execution unavailable on host.", exodus_route_name(route));
    return 0;
#endif
}
#endif
