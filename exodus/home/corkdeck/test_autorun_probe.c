// SPDX-License-Identifier: GPL-2.0-only
#include "../../runtime/autorun_probe.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { IMAGE_BYTES = 0x3000, TRAILER_BYTES = 64 };

static unsigned checks;
static unsigned failures;

static void write_u32_le(unsigned char *target, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        target[i] = (unsigned char)(value >> (8 * i));
}

static void make_image(unsigned char image[IMAGE_BYTES + TRAILER_BYTES])
{
    memset(image, 0, IMAGE_BYTES + TRAILER_BYTES);
    memcpy(image + 0x10, "NRO0", 4);
    write_u32_le(image + 0x18, IMAGE_BYTES);
    for (unsigned i = 0; i < 3; ++i) {
        write_u32_le(image + 0x20 + i * 8, i * 0x1000);
        write_u32_le(image + 0x24 + i * 8, 0x1000);
    }
}

static void expect_status(const char *name, const char *path,
                          enum exodus_autorun_status expected)
{
    enum exodus_autorun_status actual = exodus_autorun_probe(path);
    ++checks;
    if (actual != expected) {
        fprintf(stderr, "%s: expected status %d, got %d\n", name, (int)expected, (int)actual);
        ++failures;
    }
}

static void expect_image(const char *name, const char *path, const unsigned char *image,
                         size_t length, enum exodus_autorun_status expected)
{
    FILE *output = fopen(path, "wb");
    assert(output);
    assert(fwrite(image, 1, length, output) == length);
    assert(fclose(output) == 0);
    expect_status(name, path, expected);
}

int main(int argc, char **argv)
{
    /* The caller supplies a disposable, initially absent fixture path and its directory. */
    assert(argc == 3);
    const char *path = argv[1];
    unsigned char image[IMAGE_BYTES + TRAILER_BYTES];
    expect_status("missing file", path, EXODUS_AUTORUN_MISSING);
    expect_status("directory", argv[2], EXODUS_AUTORUN_UNREADABLE);

    make_image(image);
    expect_image("ordinary NRO", path, image, IMAGE_BYTES, EXODUS_AUTORUN_READY);
    memcpy(image + IMAGE_BYTES, "ASET", 4);
    expect_image("appended assets", path, image, sizeof(image), EXODUS_AUTORUN_READY);

    make_image(image);
    write_u32_le(image + 0x38, 0x100000);
    expect_image("BSS needs no file payload", path, image, IMAGE_BYTES, EXODUS_AUTORUN_READY);
    write_u32_le(image + 0x38, 1);
    expect_image("unaligned BSS is page padded", path, image, IMAGE_BYTES, EXODUS_AUTORUN_READY);

    /* Exercise the uint32 boundary using only a small on-disk image, not a huge allocation.
     * Structural acceptance of the last in-range page does not promise available heap. */
    write_u32_le(image + 0x38, 0xffffc000u);
    expect_image("last representable mapped page", path, image, IMAGE_BYTES, EXODUS_AUTORUN_READY);
    write_u32_le(image + 0x38, 0xffffc001u);
    expect_image("first page-rounding overflow", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x38, 0xffffcfffu);
    expect_image("UINT32_MAX occupied bytes", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x38, 0xffffd000u);
    expect_image("first image plus BSS overflow", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x38, UINT32_MAX);
    expect_image("maximum BSS field", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);

    make_image(image);
    write_u32_le(image + 0x2c, 0);
    expect_image("empty read-only segment", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    /* An otherwise contiguous image must also reject the empty read-only segment. */
    write_u32_le(image + 0x18, 0x2000);
    write_u32_le(image + 0x30, 0x1000);
    expect_image("packed image with empty read-only segment", path, image, 0x2000,
                 EXODUS_AUTORUN_INVALID_NRO);

    make_image(image);
    write_u32_le(image + 0x34, 0);
    expect_image("empty data and BSS range", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x38, 1);
    expect_image("BSS-backed writable range retains admission", path, image, IMAGE_BYTES,
                 EXODUS_AUTORUN_READY);

    make_image(image);
    expect_image("truncated header", path, image, 0x7f, EXODUS_AUTORUN_INVALID_NRO);
    expect_image("truncated payload", path, image, IMAGE_BYTES - 1, EXODUS_AUTORUN_INVALID_NRO);
    image[0x10] = 'X';
    expect_image("wrong magic", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);

    make_image(image);
    write_u32_le(image + 0x18, 0x40);
    expect_image("image smaller than header", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x18, IMAGE_BYTES + 1);
    expect_image("unaligned image", path, image, sizeof(image), EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x18, IMAGE_BYTES + 0x1000);
    expect_image("image extends beyond file", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);

    make_image(image);
    write_u32_le(image + 0x24, 0);
    expect_image("empty text segment", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    make_image(image);
    write_u32_le(image + 0x28, 0);
    expect_image("overlapping segments", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    write_u32_le(image + 0x28, 0x1001);
    expect_image("unaligned segment", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);
    make_image(image);
    write_u32_le(image + 0x34, UINT32_MAX);
    expect_image("oversized segment", path, image, IMAGE_BYTES, EXODUS_AUTORUN_INVALID_NRO);

    assert(remove(path) == 0);
    printf("NRO preflight: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
