#include "page_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum { WORD_RUN_BYTES = 41 };

static void fill_pixel_page(uint8_t *page)
{
    static const uint8_t pixel[] = {0x44, 0x77, 0xaa, 0xff};
    for (size_t offset = 0; offset < EXODUS_PAGE_BYTES; offset += sizeof(pixel))
        memcpy(page + offset, pixel, sizeof(pixel));
}

static void check_filled(const uint8_t *buffer, size_t size, uint8_t value)
{
    for (size_t i = 0; i < size; ++i)
        CHECK(buffer[i] == value);
}

static void check_exact_adaptive_capacity(void)
{
    uint8_t page[EXODUS_PAGE_BYTES], expected[EXODUS_PAGE_BYTES];
    fill_pixel_page(page);
    memcpy(expected, page, sizeof(expected));
    uint8_t payload[WORD_RUN_BYTES + 2];
    memset(payload, 0xa3, sizeof(payload));
    size_t ends[] = {SIZE_MAX, SIZE_MAX};
    struct exodus_page_store store = {
        .payload = payload + 1,
        .payload_capacity = WORD_RUN_BYTES - 1,
        .page_ends = ends,
        .page_capacity = 2
    };

    CHECK(!exodus_page_store_append_adaptive(&store, page));
    CHECK(store.page_count == 0 && store.payload_used == 0);
    CHECK(ends[0] == SIZE_MAX && ends[1] == SIZE_MAX);
    check_filled(payload, sizeof(payload), 0xa3);

    store.payload_capacity = WORD_RUN_BYTES;
    CHECK(!exodus_page_store_append(&store, page));
    CHECK(store.page_count == 0 && store.payload_used == 0);
    CHECK(ends[0] == SIZE_MAX && ends[1] == SIZE_MAX);
    check_filled(payload, sizeof(payload), 0xa3);

    CHECK(exodus_page_store_append_adaptive(&store, page));
    CHECK(store.page_count == 1 && store.payload_used == WORD_RUN_BYTES);
    CHECK(ends[0] == WORD_RUN_BYTES && ends[1] == SIZE_MAX);
    CHECK(payload[0] == 0xa3 && payload[sizeof(payload) - 1] == 0xa3);

    /* Retained storage owns the data after the input page is reused. */
    memset(page, 0, sizeof(page));
    uint8_t restored[EXODUS_PAGE_BYTES + 2];
    memset(restored, 0xc5, sizeof(restored));
    CHECK(exodus_page_store_restore(&store, 0, restored + 1));
    CHECK(memcmp(restored + 1, expected, sizeof(expected)) == 0);
    CHECK(restored[0] == 0xc5 && restored[sizeof(restored) - 1] == 0xc5);

    uint8_t before[sizeof(payload)];
    memcpy(before, payload, sizeof(before));
    CHECK(!exodus_page_store_append_adaptive(&store, page));
    CHECK(store.page_count == 1 && store.payload_used == WORD_RUN_BYTES);
    CHECK(ends[0] == WORD_RUN_BYTES && ends[1] == SIZE_MAX);
    CHECK(memcmp(payload, before, sizeof(payload)) == 0);
}

static void check_mixed_store(void)
{
    enum {
        PAGE_COUNT = 7,
        PAYLOAD_BYTES = 8 + 72 + WORD_RUN_BYTES + 2 * EXODUS_PAGE_BYTES + 9 + 17
    };
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES] = {{0}};
    for (size_t i = 0; i < 64; ++i)
        pages[1][i] = (uint8_t)(i + 1);
    fill_pixel_page(pages[2]);
    fill_pixel_page(pages[3]);
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i) {
        pages[4][i] = (uint8_t)i;
        pages[5][i] = (uint8_t)(1 + i % 8);
        pages[6][i] = (uint8_t)(1 + i % 16);
    }

    uint8_t payload[PAYLOAD_BYTES + 2];
    memset(payload, 0xa3, sizeof(payload));
    size_t ends[PAGE_COUNT + 1];
    for (size_t i = 0; i < PAGE_COUNT + 1; ++i)
        ends[i] = SIZE_MAX;
    struct exodus_page_store store = {
        .payload = payload + 1,
        .payload_capacity = PAYLOAD_BYTES,
        .page_ends = ends,
        .page_capacity = PAGE_COUNT
    };
    const size_t sizes[] = {8, 72, WORD_RUN_BYTES, EXODUS_PAGE_BYTES, EXODUS_PAGE_BYTES, 9, 17};
    size_t used = 0;
    for (size_t i = 0; i < PAGE_COUNT; ++i) {
        CHECK(i == 3 ? exodus_page_store_append(&store, pages[i])
                     : exodus_page_store_append_adaptive(&store, pages[i]));
        used += sizes[i];
        CHECK(ends[i] == used && store.payload_used == used && store.page_count == i + 1);
    }
    CHECK(used == PAYLOAD_BYTES && ends[PAGE_COUNT] == SIZE_MAX);
    CHECK(payload[0] == 0xa3 && payload[sizeof(payload) - 1] == 0xa3);

    uint8_t restored[EXODUS_PAGE_BYTES + 2];
    memset(restored, 0xc5, sizeof(restored));
    for (size_t i = PAGE_COUNT; i > 0; --i) {
        CHECK(exodus_page_store_restore(&store, i - 1, restored + 1));
        CHECK(memcmp(restored + 1, pages[i - 1], EXODUS_PAGE_BYTES) == 0);
        CHECK(restored[0] == 0xc5 && restored[sizeof(restored) - 1] == 0xc5);
    }

    /* A malformed word-run page must not partially overwrite the destination. */
    memset(restored, 0xc5, sizeof(restored));
    const size_t word_run_start = ends[1];
    store.payload[word_run_start] = 2;
    CHECK(!exodus_page_store_restore(&store, 2, restored + 1));
    CHECK(!exodus_page_store_restore(&store, PAGE_COUNT, restored + 1));
    check_filled(restored, sizeof(restored), 0xc5);
    CHECK(exodus_page_store_restore(&store, 3, restored + 1));
    CHECK(memcmp(restored + 1, pages[3], EXODUS_PAGE_BYTES) == 0);
}

static void check_append_failure_preserves_store(void)
{
    uint8_t page[EXODUS_PAGE_BYTES];
    fill_pixel_page(page);
    /* Exercise independent payload and index exhaustion after a valid append. */
    for (unsigned full_index = 0; full_index < 2; ++full_index) {
        uint8_t payload[2 * WORD_RUN_BYTES + 2], before[sizeof(payload)];
        memset(payload, 0xa3, sizeof(payload));
        size_t ends[] = {SIZE_MAX, SIZE_MAX, SIZE_MAX};
        size_t ends_before[sizeof(ends) / sizeof(ends[0])];
        struct exodus_page_store store = {
            .payload = payload + 1,
            .payload_capacity = 2 * WORD_RUN_BYTES - (full_index ? 0 : 1),
            .page_ends = ends,
            .page_capacity = full_index ? 1 : 2
        };
        CHECK(exodus_page_store_append_adaptive(&store, page));
        memcpy(before, payload, sizeof(before));
        memcpy(ends_before, ends, sizeof(ends_before));
        CHECK(!exodus_page_store_append_adaptive(&store, page));
        CHECK(!exodus_page_store_append(&store, page));
        CHECK(store.page_count == 1 && store.payload_used == WORD_RUN_BYTES);
        CHECK(memcmp(payload, before, sizeof(payload)) == 0);
        CHECK(memcmp(ends, ends_before, sizeof(ends)) == 0);
        uint8_t restored[EXODUS_PAGE_BYTES];
        CHECK(exodus_page_store_restore(&store, 0, restored));
        CHECK(memcmp(restored, page, sizeof(restored)) == 0);
    }
}

static void check_invalid_store(void)
{
    uint8_t page[EXODUS_PAGE_BYTES] = {0};
    uint8_t payload[WORD_RUN_BYTES];
    memset(payload, 0xa3, sizeof(payload));
    size_t end = SIZE_MAX;
    const struct exodus_page_store original = {
        .payload = payload,
        .payload_capacity = sizeof(payload),
        .page_ends = &end,
        .page_capacity = 1
    };
    struct exodus_page_store store = original;
    CHECK(!exodus_page_store_append_adaptive(NULL, page));
    CHECK(!exodus_page_store_append_adaptive(&store, NULL));
    store.payload = NULL;
    CHECK(!exodus_page_store_append_adaptive(&store, page));
    store = original;
    store.page_ends = NULL;
    CHECK(!exodus_page_store_append_adaptive(&store, page));
    store = original;
    store.payload_used = store.payload_capacity + 1;
    CHECK(!exodus_page_store_append_adaptive(&store, page));
    CHECK(store.payload_used == store.payload_capacity + 1 && store.page_count == 0);
    store = original;
    store.page_count = store.page_capacity + 1;
    CHECK(!exodus_page_store_append_adaptive(&store, page));
    CHECK(store.page_count == store.page_capacity + 1 && store.payload_used == 0);
    CHECK(end == SIZE_MAX);
    check_filled(payload, sizeof(payload), 0xa3);
}

/* Equal-size replacements retain every index entry and unrelated byte,
 * even at exact payload capacity. Reject malformed tails before writing. */
static void check_equal_size_replace(void)
{
    enum { N=16, SLOT=WORD_RUN_BYTES, CAP=N*SLOT };
    uint8_t payload[CAP+2], before[CAP+2], page[EXODUS_PAGE_BYTES];
    uint8_t restored[EXODUS_PAGE_BYTES];
    size_t ends[N+1], saved[N+1];
    memset(payload,0xa5,sizeof payload);
    for(size_t i=0;i<N+1;i++)ends[i]=SIZE_MAX;
    struct exodus_page_store s={
        .payload=payload+1,.payload_capacity=CAP,.page_ends=ends,.page_capacity=N
    };
    for(size_t i=0;i<N;i++){
        memset(page,(int)(i+1),sizeof page);
        CHECK(exodus_page_store_append_adaptive(&s,page));
    }
    CHECK(s.payload_used==CAP&&s.page_count==N);
    memcpy(saved,ends,sizeof ends);
    for(unsigned round=0;round<32;round++){
        for(size_t index=0;index<N;index++){
            memcpy(before,payload,sizeof payload);
            memset(page,(int)(64+round),sizeof page);
            CHECK(exodus_page_store_replace_adaptive(&s,index,page));
            CHECK(!memcmp(ends,saved,sizeof ends));
            CHECK(s.payload_used==CAP&&s.page_count==N);
            CHECK(!memcmp(before,payload,1+index*SLOT));
            CHECK(!memcmp(before+1+(index+1)*SLOT,payload+1+(index+1)*SLOT,
                          sizeof payload-1-(index+1)*SLOT));
            CHECK(exodus_page_store_restore(&s,index,restored));
            CHECK(!memcmp(page,restored,sizeof page));
        }
    }
    /* Full index validation remains mandatory even for equal-length edits. */
    ends[N-1]=ends[N-2];
    memcpy(saved,ends,sizeof ends);memcpy(before,payload,sizeof payload);
    memset(page,0xe1,sizeof page);
    CHECK(!exodus_page_store_replace_adaptive(&s,0,page));
    CHECK(!memcmp(saved,ends,sizeof ends)&&!memcmp(before,payload,sizeof payload));
    CHECK(s.payload_used==CAP&&s.page_count==N);
}

int main(void)
{
    check_equal_size_replace();
    check_exact_adaptive_capacity();
    check_mixed_store();
    check_append_failure_preserves_store();
    check_invalid_store();
    puts("Adaptive packed page store tests passed");
    return 0;
}
