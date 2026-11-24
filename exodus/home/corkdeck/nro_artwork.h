/* Portable, bounded NRO ASET icon reader. No writes or executable loading. */
#ifndef EXODUS_NRO_ARTWORK_H
#define EXODUS_NRO_ARTWORK_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../../runtime/autorun_probe.h"

#define EXODUS_NRO_ICON_LIMIT (128u * 1024u)
static inline uint32_t exodus_asset_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static inline uint64_t exodus_asset_u64(const unsigned char *p)
{
    return (uint64_t)exodus_asset_u32(p) | (uint64_t)exodus_asset_u32(p+4)<<32;
}

/* All ASET section offsets are relative to the start of ASET. */
static inline int exodus_nro_icon_range(const unsigned char *header, uint64_t available,
                                         uint64_t *offset, size_t *length)
{
    if (available < 56 || memcmp(header, "ASET", 4) || exodus_asset_u32(header+4)) return 0;
    uint64_t off = exodus_asset_u64(header+8), len = exodus_asset_u64(header+16);
    if (off < 56 || len < 8 || len > EXODUS_NRO_ICON_LIMIT ||
        off > available || len > available-off) return 0;
    *offset = off;
    *length = (size_t)len;
    return 1;
}

/* Caller owns the returned buffer. The NRO is never executed. */
static inline unsigned char *exodus_read_nro_icon(const char *path, size_t *length)
{
    *length = 0;
    if (exodus_autorun_probe(path) != EXODUS_AUTORUN_READY) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char header[56], size_bytes[4], *image = NULL;
    uint64_t offset;
    size_t len;
    long file_size;
    if (fseek(f, 0, SEEK_END) || (file_size=ftell(f)) < 0 ||
        fseek(f, 0x18, SEEK_SET) || fread(size_bytes, 1, 4, f) != 4) goto done;
    uint64_t base = exodus_asset_u32(size_bytes);
    if (base < 0x80 || base > (uint64_t)file_size ||
        (uint64_t)file_size-base < sizeof(header) ||
        fseek(f, (long)base, SEEK_SET) || fread(header, 1, sizeof(header), f) != sizeof(header) ||
        !exodus_nro_icon_range(header, (uint64_t)file_size-base, &offset, &len) ||
        offset > (uint64_t)LONG_MAX-base || fseek(f, (long)(base+offset), SEEK_SET)) goto done;
    image = malloc(len);
    if (!image) goto done;
    if (fread(image, 1, len, f) != len) { free(image); image=NULL; goto done; }
    *length=len;
done:
    fclose(f);
    return image;
}
#endif
