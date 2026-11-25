#include "page_snapshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "snapshot check failed at line %d: %s\n", __LINE__, #condition); \
    exit(EXIT_FAILURE); \
} } while (0)

static size_t round_trip(const uint8_t *page, const uint8_t *base, uint8_t *frame)
{
    uint8_t decoded[EXODUS_PAGE_BYTES];
    size_t size = exodus_snapshot_encode(page, base, frame);
    CHECK(size > 1 && size <= EXODUS_SNAPSHOT_MAX_BYTES);
    CHECK(exodus_snapshot_decode(frame, size, base, decoded));
    CHECK(memcmp(page, decoded, sizeof(decoded)) == 0);
    return size;
}

static void require_rejected(const uint8_t *frame, size_t size, const uint8_t *base)
{
    uint8_t decoded[EXODUS_PAGE_BYTES];
    memset(decoded, 0x5a, sizeof(decoded));
    CHECK(!exodus_snapshot_decode(frame, size, base, decoded));
    for (size_t i = 0; i < sizeof(decoded); ++i)
        CHECK(decoded[i] == 0x5a);
}

int main(void)
{
    uint8_t base[EXODUS_PAGE_BYTES], page[EXODUS_PAGE_BYTES], in_place[EXODUS_PAGE_BYTES];
    uint8_t frame[EXODUS_SNAPSHOT_MAX_BYTES], standalone[EXODUS_SNAPSHOT_MAX_BYTES];
    for (size_t i = 0; i < sizeof(base); ++i)
        base[i] = (uint8_t)i;

    /* The repeated byte ramp has CRC-32 a2912082, independently checkable
     * with zlib.crc32(bytes(range(256)) * 16). */
    size_t identical_size = round_trip(base, base, frame);
    const uint8_t identical_header[] = {1, 0x82, 0x20, 0x91, 0xa2};
    CHECK(identical_size == sizeof(identical_header) + 8);
    CHECK(memcmp(frame, identical_header, sizeof(identical_header)) == 0);
    memcpy(page, base, sizeof(page));
    page[137] ^= 0x80;
    size_t full_size = round_trip(page, NULL, standalone);
    size_t ram_size = round_trip(page, base, frame);
    CHECK(frame[0] == 1 && ram_size < full_size);
    printf("One changed RAM byte: %zu standalone, %zu reference bytes\n", full_size, ram_size);
    for (size_t i = 0; i < ram_size; ++i)
        require_rejected(frame, i, base);
    require_rejected(frame, EXODUS_SNAPSHOT_MAX_BYTES + 1, base);
    require_rejected(frame, ram_size, NULL);
    require_rejected(frame, ram_size, page);
    memcpy(in_place, base, sizeof(base));
    CHECK(exodus_snapshot_decode(frame, ram_size, in_place, in_place));
    CHECK(memcmp(in_place, page, sizeof(page)) == 0);
    frame[0] = 255;
    require_rejected(frame, ram_size, base);

    /* Change a 4x4 region of a 32x32 RGBA staging image. */
    memcpy(page, base, sizeof(page));
    for (size_t y = 12; y < 16; ++y)
        memset(page + (y * 32 + 8) * 4, 0x7f, 4 * 4);
    full_size = round_trip(page, NULL, standalone);
    size_t rgba_size = round_trip(page, base, frame);
    CHECK(frame[0] == 1 && rgba_size < full_size);
    printf("One changed RGBA patch: %zu standalone, %zu reference bytes\n", full_size, rgba_size);

    /* Unrelated dense pages should keep a reference-independent frame. */
    uint32_t state = 1;
    for (size_t i = 0; i < sizeof(page); ++i) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        page[i] = (uint8_t)(state >> 24);
    }
    full_size = round_trip(page, NULL, standalone);
    size_t chosen_size = round_trip(page, base, frame);
    CHECK(chosen_size == full_size && frame[0] == 0);
    CHECK(memcmp(frame, standalone, full_size) == 0);
    CHECK(exodus_snapshot_decode(frame, chosen_size, NULL, in_place));
    CHECK(memcmp(in_place, page, sizeof(page)) == 0);
    printf("Unrelated page: %zu standalone, %zu selected bytes\n", full_size, chosen_size);

    memset(page, 0, sizeof(page));
    CHECK(round_trip(page, base, frame) == 9 && frame[0] == 0);
    return EXIT_SUCCESS;
}
