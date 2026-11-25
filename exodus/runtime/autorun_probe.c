// SPDX-License-Identifier: GPL-2.0-only
#include "autorun_probe.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* NroStart followed by NroHeader, as defined by libnx. */
enum {
    NRO_HEADER_BYTES = 0x80,
    NRO_MAGIC_OFFSET = 0x10,
    NRO_IMAGE_SIZE_OFFSET = 0x18,
    NRO_SEGMENTS_OFFSET = 0x20,
    NRO_SEGMENT_BYTES = 8,
    NRO_SEGMENT_COUNT = 3,
    NRO_PAGE_BYTES = 0x1000,
};

static uint32_t read_u32_le(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static bool valid_nro_header(const unsigned char *header, uintmax_t file_size)
{
    if (memcmp(header + NRO_MAGIC_OFFSET, "NRO0", 4) != 0)
        return false;

    uint32_t image_size = read_u32_le(header + NRO_IMAGE_SIZE_OFFSET);
    if (image_size < NRO_HEADER_BYTES || image_size > file_size ||
        image_size % NRO_PAGE_BYTES != 0)
        return false;

    uint32_t previous_end = 0;
    for (size_t i = 0; i < NRO_SEGMENT_COUNT; ++i) {
        const unsigned char *segment = header + NRO_SEGMENTS_OFFSET + i * NRO_SEGMENT_BYTES;
        uint32_t offset = read_u32_le(segment);
        uint32_t size = read_u32_le(segment + 4);
        if ((i == 0 && (offset != 0 || size < NRO_HEADER_BYTES)) ||
            offset < previous_end || offset > image_size || size > image_size - offset ||
            offset % NRO_PAGE_BYTES != 0 || size % NRO_PAGE_BYTES != 0)
            return false;
        previous_end = offset + size;
    }
    return true;
}

enum exodus_autorun_status exodus_autorun_probe(const char *path)
{
    struct stat metadata;
    if (stat(path, &metadata) != 0) {
        if (errno == ENOENT || errno == ENOTDIR)
            return EXODUS_AUTORUN_MISSING;
        return EXODUS_AUTORUN_UNREADABLE;
    }
    if (!S_ISREG(metadata.st_mode))
        return EXODUS_AUTORUN_UNREADABLE;
    if (metadata.st_size < NRO_HEADER_BYTES)
        return EXODUS_AUTORUN_INVALID_NRO;

    FILE *input = fopen(path, "rb");
    if (!input)
        return EXODUS_AUTORUN_UNREADABLE;

    unsigned char header[NRO_HEADER_BYTES];
    size_t bytes_read = fread(header, 1, sizeof(header), input);
    bool read_failed = ferror(input) != 0;
    if (fclose(input) != 0 || read_failed)
        return EXODUS_AUTORUN_UNREADABLE;
    if (bytes_read != sizeof(header) || !valid_nro_header(header, (uintmax_t)metadata.st_size))
        return EXODUS_AUTORUN_INVALID_NRO;
    return EXODUS_AUTORUN_READY;
}
