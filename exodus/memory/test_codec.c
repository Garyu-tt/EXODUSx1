#include "page_codec.h"
#include "page_store.h"

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

static size_t check_adaptive_round_trip(const uint8_t *page, uint8_t *encoded)
{
    uint8_t decoded[EXODUS_PAGE_BYTES + 2];
    memset(encoded, 0xa3, EXODUS_PAGE_BYTES + 2);
    memset(decoded, 0xc5, sizeof(decoded));
    size_t size = exodus_page_encode_adaptive(page, encoded + 1);
    CHECK(size <= EXODUS_PAGE_BYTES);
    CHECK(encoded[0] == 0xa3 && encoded[1 + size] == 0xa3);
    CHECK(exodus_page_decode(encoded + 1, size, decoded + 1));
    CHECK(memcmp(page, decoded + 1, EXODUS_PAGE_BYTES) == 0);
    bool equal=false;
    CHECK(exodus_page_compare(encoded+1,size,page,&equal)&&equal);
    decoded[EXODUS_PAGE_BYTES]^=1;
    CHECK(exodus_page_compare(encoded+1,size,decoded+1,&equal)&&!equal);
    decoded[EXODUS_PAGE_BYTES]^=1;
    CHECK(decoded[0] == 0xc5 && decoded[sizeof(decoded) - 1] == 0xc5);
    return size;
}

static void check_rejected_encoding(const uint8_t *encoded, size_t size)
{
    uint8_t decoded[EXODUS_PAGE_BYTES];
    memset(decoded, 0x6a, sizeof(decoded));
    CHECK(!exodus_page_decode(encoded, size, decoded));
    bool equal=true;
    CHECK(!exodus_page_compare(encoded,size,decoded,&equal)&&equal);
    for (size_t i = 0; i < sizeof(decoded); ++i)
        CHECK(decoded[i] == 0x6a);
}

static void check_word_run_lengths(void)
{
    static const uint8_t first_word[] = {0x26, 0xe1, 0, 0x8c};
    static const uint8_t other_word[] = {0xfa, 0, 0x39, 0x65};
    const size_t page_words = EXODUS_PAGE_BYTES / sizeof(first_word);

    for (size_t count = 2; count <= 128; ++count) {
        uint8_t encoded[EXODUS_PAGE_BYTES];
        uint8_t restored[EXODUS_PAGE_BYTES + 2];
        uint8_t *stream = encoded + 1;
        size_t size = 0;
        stream[size++] = 1;
        stream[size++] = (uint8_t)(0x80 | (count - 1));
        memcpy(stream + size, first_word, sizeof(first_word));
        size += sizeof(first_word);
        for (size_t remaining = page_words - count; remaining > 0;) {
            size_t chunk = remaining > 128 ? 128 : remaining;
            stream[size++] = chunk == 1 ? 0 : (uint8_t)(0x80 | (chunk - 1));
            memcpy(stream + size, other_word, sizeof(other_word));
            size += sizeof(other_word);
            remaining -= chunk;
        }
        if (size % 2 == 0)
            stream[size++] = 0;

        memset(restored, 0x6a, sizeof(restored));
        CHECK(exodus_page_decode(stream, size, restored + 1));
        bool equal=false;
        CHECK(exodus_page_compare(stream,size,restored+1,&equal)&&equal);
        restored[EXODUS_PAGE_BYTES]^=1;
        CHECK(exodus_page_compare(stream,size,restored+1,&equal)&&!equal);
        restored[EXODUS_PAGE_BYTES]^=1;
        CHECK(restored[0] == 0x6a && restored[sizeof(restored) - 1] == 0x6a);
        for (size_t i = 0; i < page_words; ++i) {
            const uint8_t *expected = i < count ? first_word : other_word;
            CHECK(memcmp(restored + 1 + i * sizeof(first_word), expected, sizeof(first_word)) == 0);
        }
    }
}

static void check_adaptive(void)
{
    const uint8_t pixel[] = {0x53, 0x71, 0x95, 0xff};
    uint8_t page[EXODUS_PAGE_BYTES] = {0};
    uint8_t encoded[EXODUS_PAGE_BYTES + 2];

    CHECK(check_adaptive_round_trip(page, encoded) == 8);
    for (size_t i = 0; i < BLOCK_BYTES; ++i)
        page[i] = (uint8_t)(i + 1);
    CHECK(check_adaptive_round_trip(page, encoded) == 8 + BLOCK_BYTES);
    for (size_t i = 0; i < sizeof(page); ++i)
        page[i] = (uint8_t)i;
    CHECK(check_adaptive_round_trip(page, encoded) == EXODUS_PAGE_BYTES);

    for (size_t i = 0; i < sizeof(page); i += sizeof(pixel))
        memcpy(page + i, pixel, sizeof(pixel));
    CHECK(exodus_page_encode(page, encoded) == EXODUS_PAGE_BYTES);
    CHECK(memcmp(page, encoded, sizeof(page)) == 0);
    size_t size = check_adaptive_round_trip(page, encoded);
    CHECK(size == 41 && encoded[1] == 1);
    for (size_t i = 2; i < size + 1; i += 5) {
        CHECK(encoded[i] == 0xff);
        CHECK(memcmp(encoded + i + 1, pixel, sizeof(pixel)) == 0);
    }
    for (size_t i = 0; i < size; ++i)
        check_rejected_encoding(encoded + 1, i);
    check_rejected_encoding(encoded + 1, size + 2);
    encoded[1] = 3;
    check_rejected_encoding(encoded + 1, size);
    encoded[1] = 1;
    encoded[2] = 0x80;
    check_rejected_encoding(encoded + 1, size);

    /* 128 literals, a repeat crossing its 128-word limit, then other runs. */
    for (size_t i = 0; i < 128 * sizeof(pixel); ++i)
        page[i] = (uint8_t)i;
    memset(page + 128 * sizeof(pixel), 0x7c, 129 * sizeof(pixel));
    size = check_adaptive_round_trip(page, encoded);
    CHECK(size == 555 && encoded[size] == 0);
    encoded[size] = 1;
    check_rejected_encoding(encoded + 1, size);

    /* Seven full repeats, one 127-word repeat and a two-word overrun. */
    uint8_t overrun[47] = {1};
    for (size_t i = 1; i < sizeof(overrun) - 1; i += 5)
        overrun[i] = 0xff;
    overrun[36] = 0xfe;
    overrun[41] = 0x81;
    check_rejected_encoding(overrun, sizeof(overrun));
}

static void check_repeated_tiles(void)
{
    uint8_t page[EXODUS_PAGE_BYTES];
    uint8_t encoded[EXODUS_PAGE_BYTES + 2];
    static const size_t tile_sizes[] = {8, 16};

    for (size_t i = 0; i < sizeof(tile_sizes) / sizeof(tile_sizes[0]); ++i) {
        size_t tile_size = tile_sizes[i];
        for (size_t offset = 0; offset < sizeof(page); ++offset)
            page[offset] = (uint8_t)(1 + offset % tile_size);
        CHECK(exodus_page_encode(page, encoded) == EXODUS_PAGE_BYTES);
        CHECK(memcmp(page, encoded, sizeof(page)) == 0);

        size_t encoded_size = check_adaptive_round_trip(page, encoded);
        CHECK(encoded_size == 1 + tile_size);
        CHECK(encoded[1] == EXODUS_PAGE_REPEATED_TILE_VERSION);
        CHECK(memcmp(encoded + 2, page, tile_size) == 0);
        check_rejected_encoding(encoded + 1, encoded_size - 2);
        check_rejected_encoding(encoded + 1, encoded_size + 2);

        /* Even a mismatch at the final byte must defeat this representation. */
        page[sizeof(page) - 1] ^= 0xff;
        CHECK(check_adaptive_round_trip(page, encoded) == EXODUS_PAGE_BYTES);
    }
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

static void check_page_store(void)
{
    uint8_t pages[3][EXODUS_PAGE_BYTES] = {0};
    pages[1][EXODUS_PAGE_BYTES - 1] = 0x9a;
    memset(pages[2], 0x5b, sizeof(pages[2]));

    uint8_t payload[8 + 72 + EXODUS_PAGE_BYTES + 2];
    memset(payload, 0xa3, sizeof(payload));
    size_t ends[] = {0, 0, 0, SIZE_MAX};
    struct exodus_page_store store = {
        .payload = payload + 1,
        .payload_capacity = sizeof(payload) - 2,
        .page_ends = ends,
        .page_capacity = sizeof(ends) / sizeof(ends[0])
    };
    CHECK(exodus_page_store_append(&store, pages[0]));
    CHECK(exodus_page_store_append(&store, pages[1]));
    CHECK(exodus_page_store_append(&store, pages[2]));
    CHECK(ends[0] == 8 && ends[1] == 80 && ends[2] == store.payload_capacity);
    CHECK(store.payload_used == store.payload_capacity && store.page_count == 3);

    CHECK(!exodus_page_store_append(&store, pages[0]));
    CHECK(store.payload_used == store.payload_capacity && store.page_count == 3);
    CHECK(ends[3] == SIZE_MAX);
    CHECK(payload[0] == 0xa3 && payload[sizeof(payload) - 1] == 0xa3);

    uint8_t restored[EXODUS_PAGE_BYTES];
    for (size_t i = store.page_count; i > 0; --i) {
        CHECK(exodus_page_store_restore(&store, i - 1, restored));
        CHECK(memcmp(restored, pages[i - 1], sizeof(restored)) == 0);
    }
    memset(restored, 0xc5, sizeof(restored));
    CHECK(!exodus_page_store_restore(&store, store.page_count, restored));
    ends[1] = ends[0] - 1;
    CHECK(!exodus_page_store_restore(&store, 1, restored));
    ends[1] = store.payload_used + 1;
    CHECK(!exodus_page_store_restore(&store, 1, restored));
    ends[1] = 16;
    CHECK(!exodus_page_store_restore(&store, 1, restored));
    for (size_t i = 0; i < sizeof(restored); ++i)
        CHECK(restored[i] == 0xc5);
}

static void check_page_store_index_capacity(void)
{
    uint8_t page[EXODUS_PAGE_BYTES] = {0};
    uint8_t payload[16];
    memset(payload, 0xa3, sizeof(payload));
    size_t end = 0;
    struct exodus_page_store store = {
        .payload = payload,
        .payload_capacity = sizeof(payload),
        .page_ends = &end,
        .page_capacity = 1
    };
    CHECK(exodus_page_store_append(&store, page));
    CHECK(!exodus_page_store_append(&store, page));
    CHECK(store.payload_used == 8 && store.page_count == 1 && end == 8);
    for (size_t i = store.payload_used; i < sizeof(payload); ++i)
        CHECK(payload[i] == 0xa3);
}

/* A late malformed command must be rejected even after an early mismatch.
 * Exercise both padding and repeated/literal command validation. */
static void check_compare_invalid_tail(void)
{
    uint8_t stream[41]={1}, page[EXODUS_PAGE_BYTES]={0};
    for(size_t i=0;i<8;i++){
        stream[1+i*5]=255;
        stream[2+i*5]=1;
    }
    bool equal=true;
    CHECK(exodus_page_compare(stream,sizeof stream,page,&equal)&&!equal);
    stream[36]=128; /* Invalid repeat of one word, after the first mismatch. */
    equal=true;
    CHECK(!exodus_page_compare(stream,sizeof stream,page,&equal)&&equal);
    CHECK(!exodus_page_compare(NULL,sizeof stream,page,&equal)&&equal);
    CHECK(!exodus_page_compare(stream,0,page,&equal)&&equal);
    CHECK(!exodus_page_compare(stream,SIZE_MAX,page,&equal)&&equal);
}

/* Compare against the original decoder for valid and mutated wire records.
 * Mutations may produce another valid record, so do not assume rejection. */
static void check_compare_differential(void)
{
    uint8_t page[EXODUS_PAGE_BYTES], encoded[EXODUS_PAGE_BYTES + 2];
    uint8_t decoded[EXODUS_PAGE_BYTES], candidate[EXODUS_PAGE_BYTES];
    uint8_t *wire = encoded + 1;
    for (unsigned kind = 0; kind < 6; ++kind) {
        for (size_t i = 0; i < sizeof page; ++i) {
            switch (kind) {
            case 0: page[i] = (uint8_t)next_random(); break;
            case 1: page[i] = i / 64 % 3 == 0 ? (uint8_t)next_random() : 0; break;
            case 2: page[i] = 0x71; break;
            case 3: page[i] = (uint8_t)(1 + i % 8); break;
            case 4: page[i] = (uint8_t)(1 + i % 16); break;
            default: page[i] = i % 4 == 0 ? (uint8_t)(i / 4 % 16) :
                               i % 4 == 2 ? 0x34 : i % 4 == 3 ? 0x12 : 0; break;
            }
        }
        size_t size = exodus_page_encode_adaptive(page, wire);
        CHECK(size > 0 && size <= sizeof page);
        bool equal = false;
        CHECK(exodus_page_compare(wire,size,page,&equal) && equal);
        memcpy(candidate,page,sizeof candidate);
        /* Exercise every byte position, not just the end of a page. */
        for (size_t i = 0; i < sizeof candidate; ++i) {
            candidate[i] ^= 0x80;
            CHECK(exodus_page_compare(wire,size,candidate,&equal) && !equal);
            candidate[i] ^= 0x80;
        }
        for (size_t j = 0; j < 256; ++j) {
            size_t pos = j < size && j < 16 ? j : next_random() % size;
            uint8_t bit = (uint8_t)(1u << (next_random() % 8));
            wire[pos] ^= bit;
            size_t n = j % 4 == 0 ? next_random() % (size + 1) : size;
            bool valid = exodus_page_decode(wire,n,decoded);
            equal = true;
            CHECK(exodus_page_compare(wire,n,page,&equal) == valid);
            if (valid) {
                CHECK(equal == (memcmp(page,decoded,sizeof page) == 0));
                CHECK(exodus_page_compare(wire,n,decoded,&equal) && equal);
            } else {
                CHECK(equal);
            }
            wire[pos] ^= bit;
        }
    }
}

int main(void)
{
    check_compare_differential();
    check_compare_invalid_tail();
    check_adaptive();
    check_repeated_tiles();
    check_word_run_lengths();
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

    check_page_store();
    check_page_store_index_capacity();
    puts("Codec and packed page store tests passed");
    return 0;
}
