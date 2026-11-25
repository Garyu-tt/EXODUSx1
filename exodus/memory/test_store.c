#include "page_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum { PAGE_CAPACITY = 8, PAYLOAD_CAPACITY = 4 * EXODUS_PAGE_BYTES };

struct fixture {
    uint8_t payload[PAYLOAD_CAPACITY + 2];
    size_t ends[PAGE_CAPACITY + 2];
    struct exodus_page_store store;
};

static void initialize(struct fixture *fixture, size_t capacity)
{
    memset(fixture->payload, 0xa3, sizeof(fixture->payload));
    for (size_t i = 0; i < sizeof(fixture->ends) / sizeof(fixture->ends[0]); ++i)
        fixture->ends[i] = SIZE_MAX;
    fixture->store = (struct exodus_page_store){
        .payload = fixture->payload + 1,
        .payload_capacity = capacity,
        .page_ends = fixture->ends + 1,
        .page_capacity = PAGE_CAPACITY
    };
}

static void make_page(uint8_t *page, unsigned layout, unsigned value)
{
    memset(page, 0, EXODUS_PAGE_BYTES);
    if (layout == 3) {
        for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i)
            page[i] = (uint8_t)(1 + (i + value) % 255);
    } else if (layout != 0) {
        unsigned blocks = layout == 1 ? 1 : 1 + value % 17;
        for (unsigned i = 0; i < blocks; ++i)
            page[((i * 7 + value) % 64) * 64] = (uint8_t)(1 + value % 255);
    }
}

static void check_unchanged(const struct fixture *fixture, const struct fixture *before)
{
    CHECK(memcmp(fixture->payload, before->payload, sizeof(fixture->payload)) == 0);
    CHECK(memcmp(fixture->ends, before->ends, sizeof(fixture->ends)) == 0);
    CHECK(fixture->store.payload == before->store.payload);
    CHECK(fixture->store.page_ends == before->store.page_ends);
    CHECK(fixture->store.payload_capacity == before->store.payload_capacity);
    CHECK(fixture->store.page_capacity == before->store.page_capacity);
    CHECK(fixture->store.payload_used == before->store.payload_used);
    CHECK(fixture->store.page_count == before->store.page_count);
}

static void check_pages(const struct fixture *fixture, uint8_t pages[][EXODUS_PAGE_BYTES], size_t count)
{
    const struct exodus_page_store *store = &fixture->store;
    CHECK(store->page_count == count);
    CHECK(store->payload_used <= store->payload_capacity);
    CHECK(fixture->payload[0] == 0xa3);
    CHECK(fixture->payload[store->payload_capacity + 1] == 0xa3);
    CHECK(fixture->ends[0] == SIZE_MAX);
    for (size_t i = count + 1; i < sizeof(fixture->ends) / sizeof(fixture->ends[0]); ++i)
        CHECK(fixture->ends[i] == SIZE_MAX);
    size_t previous = 0;
    for (size_t i = 0; i < count; ++i) {
        CHECK(store->page_ends[i] > previous);
        previous = store->page_ends[i];
        uint8_t restored[EXODUS_PAGE_BYTES + 2];
        memset(restored, 0xc5, sizeof(restored));
        CHECK(exodus_page_store_restore(store, i, restored + 1));
        CHECK(memcmp(restored + 1, pages[i], EXODUS_PAGE_BYTES) == 0);
        CHECK(restored[0] == 0xc5 && restored[sizeof(restored) - 1] == 0xc5);
    }
    CHECK(previous == store->payload_used);
}

static void expect_rejected(struct fixture *fixture, size_t index, const uint8_t *page)
{
    struct fixture before = *fixture;
    CHECK(!exodus_page_store_replace(&fixture->store, index, page));
    check_unchanged(fixture, &before);
}

static void check_replacement_positions(void)
{
    for (size_t index = 0; index < 3; ++index) {
        struct fixture fixture;
        initialize(&fixture, PAYLOAD_CAPACITY);
        uint8_t pages[PAGE_CAPACITY][EXODUS_PAGE_BYTES];
        for (size_t i = 0; i < 3; ++i) {
            make_page(pages[i], 1, (unsigned)i + 10);
            CHECK(exodus_page_store_append(&fixture.store, pages[i]));
        }
        const unsigned layouts[] = {3, 2, 0, 1, 1};
        for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
            make_page(pages[index], layouts[i], (unsigned)i + 20);
            size_t old_used = fixture.store.payload_used;
            size_t old_ends[PAGE_CAPACITY + 2];
            memcpy(old_ends, fixture.ends, sizeof(old_ends));
            CHECK(exodus_page_store_replace(&fixture.store, index, pages[index]));
            check_pages(&fixture, pages, 3);
            if (i == 4) {
                CHECK(fixture.store.payload_used == old_used);
                CHECK(memcmp(fixture.ends, old_ends, sizeof(old_ends)) == 0);
            }
        }
        make_page(pages[3], 3, 72);
        CHECK(exodus_page_store_append(&fixture.store, pages[3]));
        check_pages(&fixture, pages, 4);
    }
}

static void check_capacity_and_invalid_inputs(void)
{
    struct fixture fixture;
    initialize(&fixture, EXODUS_PAGE_BYTES + 16);
    uint8_t pages[PAGE_CAPACITY][EXODUS_PAGE_BYTES] = {{0}};
    expect_rejected(&fixture, 0, pages[0]);
    for (size_t i = 0; i < 3; ++i)
        CHECK(exodus_page_store_append(&fixture.store, pages[i]));
    make_page(pages[1], 3, 45);
    CHECK(exodus_page_store_replace(&fixture.store, 1, pages[1]));
    CHECK(fixture.store.payload_used == fixture.store.payload_capacity);
    check_pages(&fixture, pages, 3);

    uint8_t replacement[EXODUS_PAGE_BYTES];
    make_page(replacement, 3, 52);
    expect_rejected(&fixture, 0, replacement);
    expect_rejected(&fixture, 3, replacement);
    expect_rejected(&fixture, SIZE_MAX, replacement);
    expect_rejected(&fixture, 0, NULL);
    CHECK(!exodus_page_store_replace(NULL, 0, replacement));

    make_page(pages[1], 0, 0);
    CHECK(exodus_page_store_replace(&fixture.store, 1, pages[1]));
    CHECK(fixture.store.payload_used == 24);
    make_page(pages[3], 1, 33);
    CHECK(exodus_page_store_append(&fixture.store, pages[3]));
    check_pages(&fixture, pages, 4);

    struct exodus_page_store valid = fixture.store;
    const size_t saved_ends[] = {8, 16, 24, 96};
    for (unsigned fault = 0; fault < 8; ++fault) {
        fixture.store = valid;
        memcpy(fixture.store.page_ends, saved_ends, sizeof(saved_ends));
        switch (fault) {
        case 0: fixture.store.page_ends[1] = 8; break;
        case 1: fixture.store.page_ends[1] = 7; break;
        case 2: fixture.store.page_ends[2] = valid.payload_used + 1; break;
        case 3: fixture.store.page_ends[3] = valid.payload_used - 1; break;
        case 4: fixture.store.page_capacity = 3; break;
        case 5: fixture.store.payload_capacity = valid.payload_used - 1; break;
        case 6: fixture.store.payload = NULL; break;
        case 7: fixture.store.page_ends = NULL; break;
        }
        expect_rejected(&fixture, 1, replacement);
    }
}

static uint32_t random_value(void)
{
    static uint32_t state = 0x58a492c1;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static void check_operation_sequences(void)
{
    struct fixture fixture;
    initialize(&fixture, 3 * EXODUS_PAGE_BYTES);
    uint8_t pages[PAGE_CAPACITY][EXODUS_PAGE_BYTES] = {{0}};
    uint8_t replacement[EXODUS_PAGE_BYTES];
    size_t count = 0;
    unsigned replacements = 0, rejected = 0;
    for (unsigned step = 0; step < 600; ++step) {
        struct fixture before = fixture;
        unsigned layout = random_value() % 4;
        make_page(replacement, layout, random_value());
        if (count == 0 || (step % 7 == 0 && count < PAGE_CAPACITY)) {
            if (exodus_page_store_append(&fixture.store, replacement))
                memcpy(pages[count++], replacement, EXODUS_PAGE_BYTES);
            else
                check_unchanged(&fixture, &before);
        } else {
            size_t index = random_value() % (count + 1);
            if (exodus_page_store_replace(&fixture.store, index, replacement)) {
                CHECK(index < count);
                memcpy(pages[index], replacement, EXODUS_PAGE_BYTES);
                ++replacements;
            } else {
                check_unchanged(&fixture, &before);
                ++rejected;
            }
        }
        check_pages(&fixture, pages, count);
    }
    CHECK(count == PAGE_CAPACITY && replacements > 100 && rejected > 10);
}

int main(void)
{
    check_replacement_positions();
    check_capacity_and_invalid_inputs();
    check_operation_sequences();
    puts("Packed page replacement tests passed");
    return 0;
}
