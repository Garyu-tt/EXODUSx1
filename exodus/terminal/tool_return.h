/* EXODUS local return contract v1. No arbitrary return target. */
#ifndef EXODUS_TOOL_RETURN_H
#define EXODUS_TOOL_RETURN_H
#include <string.h>
#define EXODUS_RETURN_FLAG "--exodus-return-home"
#define EXODUS_RETURN_PATH "sdmc:/switch/exodus-shell/exodus-shell.nro"
static inline int exodus_return_requested(int argc, char **argv)
{
    return argc == 2 && argv && argv[1] && !strcmp(argv[1], EXODUS_RETURN_FLAG);
}
#ifdef __SWITCH__
#include <switch.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
static inline uint32_t exodus_return_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 |
           (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static inline int exodus_return_preflight(void)
{
    struct stat st;
    unsigned char h[128];
    if (stat(EXODUS_RETURN_PATH,&st) || !S_ISREG(st.st_mode) || st.st_size<128) return 0;
    FILE *f=fopen(EXODUS_RETURN_PATH,"rb");
    if (!f) return 0;
    size_t n=fread(h,1,sizeof(h),f);
    int error=ferror(f);
    if (fclose(f) || error || n!=sizeof(h) || memcmp(h+16,"NRO0",4)) return 0;
    uint32_t size=exodus_return_u32(h+24), end=0;
    if (size<128 || size%4096 || size>(uint64_t)st.st_size) return 0;
    for (unsigned i=0;i<3;i++) {
        uint32_t off=exodus_return_u32(h+32+i*8), len=exodus_return_u32(h+36+i*8);
        if ((i==0 && (off || len<128)) || off<end || off>size ||
            len>size-off || off%4096 || len%4096) return 0;
        end=off+len;
    }
    return 1;
}
/* Schedule only after an explicit user exit; never on system shutdown. */
static inline int exodus_return_home(void)
{
    if (appletGetAppletType()!=AppletType_Application || !envHasNextLoad() ||
        !exodus_return_preflight()) {
        puts("EXODUS return unavailable; exiting to loader.");
        return 0;
    }
    Result rc=envSetNextLoad(EXODUS_RETURN_PATH,EXODUS_RETURN_PATH);
    if (R_FAILED(rc)) {
        printf("EXODUS return failed: 0x%08lx\n",(unsigned long)rc);
        return 0;
    }
    return 1;
}
#endif
#endif
