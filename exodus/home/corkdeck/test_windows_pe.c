// SPDX-License-Identifier: GPL-2.0-only
#include "windows_pe.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    IMAGE_SIZE = 1024,
    PE_OFFSET = 0x80,
    OPTIONAL_OFFSET = PE_OFFSET + 24,
    PE32_OPTIONAL_SIZE = 224,
    PE32_PLUS_OPTIONAL_SIZE = 240,
    PE32_TABLE_OFFSET = OPTIONAL_OFFSET + PE32_OPTIONAL_SIZE,
};

static void write_le16(unsigned char *bytes, uint16_t value)
{
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
}

static void write_le32(unsigned char *bytes, uint32_t value)
{
    write_le16(bytes, (uint16_t)value);
    write_le16(bytes + 2, (uint16_t)(value >> 16));
}

static void make_executable(unsigned char image[IMAGE_SIZE], bool pe32_plus)
{
    memset(image, 0, IMAGE_SIZE);
    memcpy(image, "MZ", 2);
    write_le32(image + 0x3c, PE_OFFSET);
    memcpy(image + PE_OFFSET, "PE\0\0", 4);
    write_le16(image + PE_OFFSET + 4, pe32_plus ? 0x8664 : 0x014c);
    write_le16(image + PE_OFFSET + 6, 1);
    uint16_t optional_size = pe32_plus ? PE32_PLUS_OPTIONAL_SIZE : PE32_OPTIONAL_SIZE;
    write_le16(image + PE_OFFSET + 20, optional_size);
    write_le16(image + PE_OFFSET + 22, 0x0002);
    write_le16(image + OPTIONAL_OFFSET, pe32_plus ? 0x20b : 0x10b);
    write_le32(image + OPTIONAL_OFFSET + 60, 512);
    write_le32(image + OPTIONAL_OFFSET + (pe32_plus ? 108 : 92), 16);
    unsigned char *section = image + OPTIONAL_OFFSET + optional_size;
    memcpy(section, ".text", 5);
    write_le32(section + 16, 512);
    write_le32(section + 20, 512);
}

static enum exodus_windows_pe_status probe(const unsigned char *bytes, size_t length)
{
    FILE *image = tmpfile();
    assert(image);
    assert(fwrite(bytes, 1, length, image) == length);
    enum exodus_windows_pe_status status = exodus_windows_pe_probe(image);
    assert(fclose(image) == 0);
    return status;
}

static void check_machine_formats(void)
{
    const struct {
        uint16_t machine;
        enum exodus_windows_pe_status pe32;
        enum exodus_windows_pe_status pe32_plus;
    } cases[] = {
        {0x014c, EXODUS_WINDOWS_PE_READY, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE}, /* x86 */
        {0x8664, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE, EXODUS_WINDOWS_PE_READY}, /* x64 */
        {0xaa64, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE, EXODUS_WINDOWS_PE_READY}, /* ARM64 */
        {0x01c4, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE}, /* ARMNT */
        {0xa641, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE}, /* ARM64EC */
        {0x0200, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE}, /* Itanium */
        {0x0000, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE, EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE}, /* Unknown */
    };
    unsigned char image[IMAGE_SIZE];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        make_executable(image, false);
        write_le16(image + PE_OFFSET + 4, cases[i].machine);
        assert(probe(image, sizeof(image)) == cases[i].pe32);
        make_executable(image, true);
        write_le16(image + PE_OFFSET + 4, cases[i].machine);
        assert(probe(image, sizeof(image)) == cases[i].pe32_plus);
    }
}

int main(void)
{
    unsigned char image[IMAGE_SIZE];
    check_machine_formats();

    make_executable(image, false);
    assert(probe(image, 2) == EXODUS_WINDOWS_PE_INVALID); /* MZ alone. */
    assert(probe(image, PE_OFFSET + 23) == EXODUS_WINDOWS_PE_INVALID);
    assert(probe(image, PE32_TABLE_OFFSET + 39) == EXODUS_WINDOWS_PE_INVALID);
    assert(probe(image, sizeof(image) - 1) == EXODUS_WINDOWS_PE_INVALID);
    write_le16(image + PE_OFFSET + 4, 0x01c4);
    assert(probe(image, sizeof(image) - 1) == EXODUS_WINDOWS_PE_INVALID);

    image[PE_OFFSET] = 'N'; /* Legacy NE executable. */
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    make_executable(image, false);
    write_le32(image + 0x3c, UINT32_MAX);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);

    make_executable(image, true);
    write_le16(image + PE_OFFSET + 20, 96); /* PE32 header too small for PE32+. */
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    make_executable(image, false);
    write_le16(image + OPTIONAL_OFFSET, 0x107); /* ROM image. */
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    make_executable(image, false);
    write_le32(image + OPTIONAL_OFFSET + 92, 17); /* Directory extends past header. */
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    make_executable(image, false);
    write_le32(image + OPTIONAL_OFFSET + 60, PE32_TABLE_OFFSET + 39);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);

    make_executable(image, false);
    write_le16(image + PE_OFFSET + 22, 0x2002); /* DLL. */
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    write_le16(image + PE_OFFSET + 22, 0);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    make_executable(image, false);
    write_le16(image + PE_OFFSET + 6, 0);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    write_le16(image + PE_OFFSET + 6, 97);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);

    make_executable(image, false);
    write_le32(image + PE32_TABLE_OFFSET + 16, UINT32_MAX);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);
    write_le32(image + PE32_TABLE_OFFSET + 16, 512);
    write_le32(image + PE32_TABLE_OFFSET + 20, 1);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_INVALID);

    make_executable(image, false);
    write_le16(image + PE_OFFSET + 6, 2); /* An uninitialized section has no raw bytes. */
    memcpy(image + PE32_TABLE_OFFSET + 40, ".bss", 4);
    assert(probe(image, sizeof(image)) == EXODUS_WINDOWS_PE_READY);
    return 0;
}
