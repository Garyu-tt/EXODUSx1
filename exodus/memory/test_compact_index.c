#include "page_compact_index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void fill_page(uint8_t *page, size_t index)
{
    memset(page, 0, EXODUS_PAGE_BYTES);
    switch (index % 4) {
    case 1:
        page[index % EXODUS_PAGE_BYTES] = 0xa5;
        break;
    case 2:
        memset(page, (uint8_t)(1 + index % 255), EXODUS_PAGE_BYTES);
        break;
    case 3:
        for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i)
            page[i] = (uint8_t)(index + i);
        break;
    }
}

static void check_failed_build(void)
{
    uint8_t payload[EXODUS_PAGE_BYTES + 1] = {0};
    size_t offsets[] = {8, 16};
    struct exodus_page_store store = {payload, sizeof(payload), offsets, 2, 16, 2, NULL, NULL};
    struct exodus_page_compact_index index = {.page_count = 7};
    uint16_t lengths[] = {0xaaaa, 0xbbbb};
    size_t checkpoints[] = {999};

    CHECK(!exodus_page_compact_build(&store, &index, lengths, 1, checkpoints, 1));
    CHECK(!exodus_page_compact_build(&store, &index, lengths, 2, checkpoints, 0));
    offsets[1] = 8;
    CHECK(!exodus_page_compact_build(&store, &index, lengths, 2, checkpoints, 1));
    offsets[1] = 15;
    CHECK(!exodus_page_compact_build(&store, &index, lengths, 2, checkpoints, 1));
    store.page_count = 1;
    store.payload_used = sizeof(payload);
    offsets[0] = sizeof(payload);
    CHECK(!exodus_page_compact_build(&store, &index, lengths, 2, checkpoints, 1));
    CHECK(index.page_count == 7 && index.payload == NULL);
    CHECK(lengths[0] == 0xaaaa && lengths[1] == 0xbbbb && checkpoints[0] == 999);
}

int main(void)
{
    enum { PAGES = 1024, GROUPS = PAGES / EXODUS_COMPACT_INDEX_STRIDE };
    static uint8_t payload[PAGES * EXODUS_PAGE_BYTES];
    uint8_t page[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    size_t offsets[PAGES], checkpoints[GROUPS];
    uint16_t lengths[PAGES];
    struct exodus_page_store store = {payload, sizeof(payload), offsets, PAGES, 0, 0, NULL, NULL};
    struct exodus_page_compact_index index;

    CHECK(exodus_page_compact_checkpoints(0) == 0);
    CHECK(exodus_page_compact_checkpoints(EXODUS_COMPACT_INDEX_STRIDE) == 1);
    CHECK(exodus_page_compact_checkpoints(EXODUS_COMPACT_INDEX_STRIDE + 1) == 2);
    CHECK(exodus_page_compact_checkpoints(PAGES) == GROUPS);

    CHECK(exodus_page_compact_build(&store, &index, NULL, 0, NULL, 0));
    CHECK(!exodus_page_compact_restore(&index, 0, restored));
    for (size_t i = 0; i < PAGES; ++i) {
        fill_page(page, i);
        if (i % 4 == 1)
            CHECK(exodus_page_store_append(&store, page));
        else
            CHECK(exodus_page_store_append_adaptive(&store, page));
        if (i == EXODUS_COMPACT_INDEX_STRIDE) {
            CHECK(exodus_page_compact_build(&store, &index, lengths, i + 1, checkpoints, 2));
            CHECK(exodus_page_compact_restore(&index, i, restored));
            CHECK(memcmp(page, restored, sizeof(page)) == 0);
        }
    }
    CHECK(exodus_page_compact_build(&store, &index, lengths, PAGES, checkpoints, GROUPS));
    CHECK(index.payload == store.payload && index.payload_used == store.payload_used);
    CHECK(index.page_count == PAGES);
    CHECK(checkpoints[0] == 0 && checkpoints[1] == offsets[EXODUS_COMPACT_INDEX_STRIDE - 1]);
    CHECK(lengths[0] == 8 && lengths[1] == 72 &&
          lengths[2] == 41 && lengths[3] == EXODUS_PAGE_BYTES);
    CHECK(sizeof(lengths) + sizeof(checkpoints) < sizeof(offsets));

    /* The compact reader no longer uses the original offsets. */
    memset(offsets, 0, sizeof(offsets));
    for (size_t i = PAGES; i > 0; --i) {
        fill_page(page, i - 1);
        CHECK(exodus_page_compact_restore(&index, i - 1, restored));
        CHECK(memcmp(page, restored, sizeof(page)) == 0);
    }

    memset(restored, 0x6a, sizeof(restored));
    CHECK(!exodus_page_compact_restore(&index, PAGES, restored));
    lengths[EXODUS_COMPACT_INDEX_STRIDE] = EXODUS_PAGE_BYTES + 1;
    CHECK(!exodus_page_compact_restore(&index, EXODUS_COMPACT_INDEX_STRIDE + 1, restored));
    checkpoints[2] = index.payload_used + 1;
    CHECK(!exodus_page_compact_restore(&index, 2 * EXODUS_COMPACT_INDEX_STRIDE, restored));
    for (size_t i = 0; i < sizeof(restored); ++i)
        CHECK(restored[i] == 0x6a);

    check_failed_build();
    printf("%zu pages, %zu compact index bytes (was %zu)\n",
           index.page_count, sizeof(lengths) + sizeof(checkpoints), sizeof(offsets));
    return 0;
}
