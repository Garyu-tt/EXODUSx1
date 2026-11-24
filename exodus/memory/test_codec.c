#include "page_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum { BLOCK_BYTES = 64, BLOCKS = EXODUS_PAGE_BYTES / BLOCK_BYTES };
static uint32_t seed = 2839;
static uint32_t next_random(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static void check_layout(uint64_t present)
{
    uint8_t page[EXODUS_PAGE_BYTES] = {0};
    uint8_t encoded[EXODUS_PAGE_BYTES + 2], decoded[EXODUS_PAGE_BYTES + 2];
    unsigned used = 0;
    for (unsigned i = 0; i < BLOCKS; ++i) {
        if ((present >> i) & 1u) {
            for (unsigned j = 0; j < BLOCK_BYTES; ++j)
                page[i * BLOCK_BYTES + j] = (uint8_t)next_random();
            page[i * BLOCK_BYTES + BLOCK_BYTES - 1] |= 1;
            ++used;
        }
    }
    memset(encoded, 0xa3, sizeof(encoded));
    memset(decoded, 0xc5, sizeof(decoded));
    size_t size = exodus_page_encode(page, encoded + 1);
    CHECK(size == (used == BLOCKS ? EXODUS_PAGE_BYTES : 8 + used * BLOCK_BYTES));
    CHECK(encoded[0] == 0xa3 && encoded[1 + size] == 0xa3);
    if (used == BLOCKS) {
        CHECK(memcmp(page, encoded + 1, sizeof(page)) == 0);
    } else {
        for (unsigned i = 0; i < 8; ++i)
            CHECK(encoded[1 + i] == (uint8_t)(present >> (i * 8)));
    }
    CHECK(exodus_page_decode(encoded + 1, size, decoded + 1));
    CHECK(memcmp(page, decoded + 1, sizeof(page)) == 0);
    CHECK(decoded[0] == 0xc5 && decoded[sizeof(decoded) - 1] == 0xc5);
}

int main(void)
{
    check_layout(0);
    check_layout(UINT64_MAX);
    for (unsigned i = 0; i < BLOCKS; ++i) {
        check_layout(UINT64_C(1) << i);
        check_layout(~(UINT64_C(1) << i));
    }
    for (unsigned i = 0; i < 500; ++i) {
        uint64_t mask = (uint64_t)next_random() << 32;
        mask |= next_random();
        check_layout(mask);
    }

    uint8_t encoded[EXODUS_PAGE_BYTES] = {0};
    uint8_t decoded[EXODUS_PAGE_BYTES];
    memset(decoded, 0x6a, sizeof(decoded));
    encoded[7] = 0x80;
    for (size_t size = 0; size < EXODUS_PAGE_BYTES; ++size) {
        if (size != 8 + BLOCK_BYTES)
            CHECK(!exodus_page_decode(encoded, size, decoded));
    }
    CHECK(!exodus_page_decode(encoded, SIZE_MAX, decoded));
    memset(encoded, 0xff, 8);
    CHECK(!exodus_page_decode(encoded, 8 + 63 * BLOCK_BYTES, decoded));
    for (size_t i = 0; i < sizeof(decoded); ++i)
        CHECK(decoded[i] == 0x6a);
    puts("Codec round-trip and malformed-input tests passed");
    return 0;
}
