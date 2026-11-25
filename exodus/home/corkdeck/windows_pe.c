// SPDX-License-Identifier: GPL-2.0-only
#include "windows_pe.h"

#include <stdint.h>
#include <string.h>

enum {
    DOS_HEADER_SIZE = 64,
    PE_OFFSET_FIELD = 0x3c,
    PE_COFF_SIZE = 24,
    PE32_FIXED_SIZE = 96,
    PE32_PLUS_FIXED_SIZE = 112,
    SIZE_OF_HEADERS_FIELD = 60,
    DIRECTORY_SIZE = 8,
    SECTION_HEADER_SIZE = 40,
    MAX_SECTIONS = 96,
    EXECUTABLE_IMAGE = 0x0002,
    DLL_IMAGE = 0x2000,
};

static uint16_t read_le16(const unsigned char *bytes)
{
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const unsigned char *bytes)
{
    return read_le16(bytes) | (uint32_t)read_le16(bytes + 2) << 16;
}

bool exodus_windows_pe_valid(FILE *image)
{
    unsigned char dos[DOS_HEADER_SIZE], coff[PE_COFF_SIZE];
    if (fseek(image, 0, SEEK_END) != 0)
        return false;
    long end = ftell(image);
    if (end < DOS_HEADER_SIZE || fseek(image, 0, SEEK_SET) != 0 ||
        fread(dos, 1, sizeof(dos), image) != sizeof(dos) ||
        memcmp(dos, "MZ", 2) != 0)
        return false;

    uint64_t file_size = (uint64_t)end;
    uint64_t pe_offset = read_le32(dos + PE_OFFSET_FIELD);
    if (pe_offset < sizeof(dos) || pe_offset + sizeof(coff) > file_size ||
        fseek(image, (long)pe_offset, SEEK_SET) != 0 ||
        fread(coff, 1, sizeof(coff), image) != sizeof(coff) ||
        memcmp(coff, "PE\0\0", 4) != 0)
        return false;

    unsigned int sections = read_le16(coff + 6);
    size_t optional_size = read_le16(coff + 20);
    unsigned int flags = read_le16(coff + 22);
    uint64_t table_offset = pe_offset + sizeof(coff) + optional_size;
    uint64_t table_end = table_offset + sections * SECTION_HEADER_SIZE;
    if (sections == 0 || sections > MAX_SECTIONS || optional_size < PE32_FIXED_SIZE ||
        !(flags & EXECUTABLE_IMAGE) || (flags & DLL_IMAGE) || table_end > file_size)
        return false;

    unsigned char optional[PE32_PLUS_FIXED_SIZE];
    size_t read_size = optional_size < sizeof(optional) ? optional_size : sizeof(optional);
    if (fread(optional, 1, read_size, image) != read_size)
        return false;
    size_t fixed_size;
    switch (read_le16(optional)) {
    case 0x10b: fixed_size = PE32_FIXED_SIZE; break;
    case 0x20b: fixed_size = PE32_PLUS_FIXED_SIZE; break;
    default: return false;
    }
    if (optional_size < fixed_size ||
        read_le32(optional + fixed_size - 4) > (optional_size - fixed_size) / DIRECTORY_SIZE)
        return false;

    uint64_t headers_size = read_le32(optional + SIZE_OF_HEADERS_FIELD);
    if (headers_size < table_end || headers_size > file_size ||
        fseek(image, (long)table_offset, SEEK_SET) != 0)
        return false;
    for (unsigned int index = 0; index < sections; ++index) {
        unsigned char section[SECTION_HEADER_SIZE];
        if (fread(section, 1, sizeof(section), image) != sizeof(section))
            return false;
        uint64_t raw_size = read_le32(section + 16);
        uint64_t raw_offset = read_le32(section + 20);
        if (raw_size && (raw_offset < headers_size || raw_offset + raw_size > file_size))
            return false;
    }
    return true;
}
