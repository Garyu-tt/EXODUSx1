#include "page_integrity_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #expression); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

enum { PAGE_COUNT = 4, WORD_RUN_BYTES = 41, PAYLOAD_BYTES = 8 + 72 + WORD_RUN_BYTES + EXODUS_PAGE_BYTES };

static void fill_pages(uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES])
{
    memset(pages, 0, PAGE_COUNT * EXODUS_PAGE_BYTES);
    for (size_t index = 0; index < 64; ++index)
        pages[1][index] = (uint8_t)(index + 1);
    static const uint8_t pixel[] = {0x44, 0x77, 0xaa, 0xff};
    for (size_t offset = 0; offset < EXODUS_PAGE_BYTES; offset += sizeof(pixel))
        memcpy(pages[2] + offset, pixel, sizeof(pixel));
    for (size_t offset = 0; offset < EXODUS_PAGE_BYTES; ++offset)
        pages[3][offset] = (uint8_t)offset;
}

static void check_filled(const uint8_t *bytes, size_t size, uint8_t value)
{
    for (size_t index = 0; index < size; ++index)
        CHECK(bytes[index] == value);
}

static struct exodus_page_integrity_store make_store(
    uint8_t *payload, size_t payload_capacity, size_t *ends, size_t page_capacity,
    uint32_t *checksums, size_t checksum_capacity)
{
    struct exodus_page_integrity_store store = {
        .pages = {
            .payload = payload,
            .payload_capacity = payload_capacity,
            .page_ends = ends,
            .page_capacity = page_capacity,
        },
        .checksums = checksums,
        .checksum_capacity = checksum_capacity,
    };
    return store;
}

static void append_fixture(struct exodus_page_integrity_store *store,
                           uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES])
{
    CHECK(exodus_page_integrity_store_append_adaptive(store, pages[0]));
    CHECK(exodus_page_integrity_store_append(store, pages[1]));
    CHECK(exodus_page_integrity_store_append_adaptive(store, pages[2]));
    CHECK(exodus_page_integrity_store_append_adaptive(store, pages[3]));
    const size_t expected_ends[] = {8, 80, 80 + WORD_RUN_BYTES, PAYLOAD_BYTES};
    CHECK(store->pages.page_count == PAGE_COUNT);
    CHECK(store->pages.payload_used == PAYLOAD_BYTES);
    CHECK(memcmp(store->pages.page_ends, expected_ends, sizeof(expected_ends)) == 0);
}

static void check_round_trip_and_guards(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    uint8_t guarded_payload[PAYLOAD_BYTES + 2];
    size_t guarded_ends[PAGE_COUNT + 2];
    uint32_t guarded_checksums[PAGE_COUNT + 2];
    memset(guarded_payload, 0xa3, sizeof(guarded_payload));
    for (size_t index = 0; index < PAGE_COUNT + 2; ++index) {
        guarded_ends[index] = SIZE_MAX;
        guarded_checksums[index] = UINT32_C(0xa3a3a3a3);
    }
    struct exodus_page_integrity_store store = make_store(
        guarded_payload + 1, PAYLOAD_BYTES, guarded_ends + 1, PAGE_COUNT,
        guarded_checksums + 1, PAGE_COUNT);
    append_fixture(&store, pages);
    const uint32_t expected_checksums[] = {
        UINT32_C(0xc71c0011), UINT32_C(0x2fb75e35),
        UINT32_C(0xbc80941c), UINT32_C(0xa2912082),
    };
    CHECK(memcmp(store.checksums, expected_checksums, sizeof(expected_checksums)) == 0);

    uint8_t restored[EXODUS_PAGE_BYTES + 2];
    for (size_t index = 0; index < PAGE_COUNT; ++index) {
        memset(restored, 0xc5, sizeof(restored));
        CHECK(exodus_page_integrity_store_restore(&store, index, restored + 1));
        CHECK(memcmp(restored + 1, pages[index], EXODUS_PAGE_BYTES) == 0);
        CHECK(restored[0] == 0xc5 && restored[sizeof(restored) - 1] == 0xc5);
    }
    CHECK(guarded_payload[0] == 0xa3 && guarded_payload[sizeof(guarded_payload) - 1] == 0xa3);
    CHECK(guarded_ends[0] == SIZE_MAX && guarded_ends[PAGE_COUNT + 1] == SIZE_MAX);
    CHECK(guarded_checksums[0] == UINT32_C(0xa3a3a3a3));
    CHECK(guarded_checksums[PAGE_COUNT + 1] == UINT32_C(0xa3a3a3a3));
}

static void check_corruption_rejected_before_output(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    uint8_t payload[PAYLOAD_BYTES];
    size_t ends[PAGE_COUNT];
    uint32_t checksums[PAGE_COUNT];
    struct exodus_page_integrity_store store = make_store(
        payload, sizeof(payload), ends, PAGE_COUNT, checksums, PAGE_COUNT);
    append_fixture(&store, pages);

    /* The ordinary raw store has structural validation only: demonstrate the
     * baseline gap before asserting that the integrity wrapper rejects it. */
    size_t dense_start = ends[2];
    payload[dense_start + 17] ^= 1;
    uint8_t baseline[EXODUS_PAGE_BYTES];
    CHECK(exodus_page_store_restore(&store.pages, 3, baseline));
    CHECK(memcmp(baseline, pages[3], sizeof(baseline)) != 0);
    uint8_t restored[EXODUS_PAGE_BYTES];
    memset(restored, 0x6a, sizeof(restored));
    CHECK(!exodus_page_integrity_store_restore(&store, 3, restored));
    check_filled(restored, sizeof(restored), 0x6a);
    payload[dense_start + 17] ^= 1;

    /* CRC-32 detects every single-bit payload error. Exercise every retained
     * byte across zero/sparse/word-run/raw encodings. */
    size_t start = 0;
    for (size_t page_index = 0; page_index < PAGE_COUNT; ++page_index) {
        for (size_t offset = start; offset < ends[page_index]; ++offset) {
            payload[offset] ^= 1;
            memset(restored, 0x6a, sizeof(restored));
            CHECK(!exodus_page_integrity_store_restore(&store, page_index, restored));
            check_filled(restored, sizeof(restored), 0x6a);
            payload[offset] ^= 1;
        }
        start = ends[page_index];
    }

    for (size_t index = 0; index < PAGE_COUNT; ++index) {
        checksums[index] ^= 1;
        memset(restored, 0x6a, sizeof(restored));
        CHECK(!exodus_page_integrity_store_restore(&store, index, restored));
        check_filled(restored, sizeof(restored), 0x6a);
        checksums[index] ^= 1;
    }

    size_t saved_end = ends[1];
    ends[1] = ends[0];
    memset(restored, 0x6a, sizeof(restored));
    CHECK(!exodus_page_integrity_store_restore(&store, 0, restored));
    check_filled(restored, sizeof(restored), 0x6a);
    ends[1] = saved_end;
}

static void check_failure_atomicity(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    uint8_t payload[PAYLOAD_BYTES + 8];
    size_t ends[PAGE_COUNT + 1];
    uint32_t checksums[PAGE_COUNT + 1];
    memset(payload, 0xa3, sizeof(payload));
    for (size_t index = 0; index < PAGE_COUNT + 1; ++index) {
        ends[index] = SIZE_MAX;
        checksums[index] = UINT32_C(0xa3a3a3a3);
    }
    struct exodus_page_integrity_store store = make_store(
        payload, PAYLOAD_BYTES, ends, PAGE_COUNT + 1, checksums, PAGE_COUNT);
    append_fixture(&store, pages);

    uint8_t payload_before[sizeof(payload)];
    size_t ends_before[PAGE_COUNT + 1];
    uint32_t checksums_before[PAGE_COUNT + 1];
    memcpy(payload_before, payload, sizeof(payload));
    memcpy(ends_before, ends, sizeof(ends));
    memcpy(checksums_before, checksums, sizeof(checksums));
    CHECK(!exodus_page_integrity_store_append_adaptive(&store, pages[0]));
    CHECK(store.pages.page_count == PAGE_COUNT && store.pages.payload_used == PAYLOAD_BYTES);
    CHECK(memcmp(payload, payload_before, sizeof(payload)) == 0);
    CHECK(memcmp(ends, ends_before, sizeof(ends)) == 0);
    CHECK(memcmp(checksums, checksums_before, sizeof(checksums)) == 0);

    store.checksum_capacity = PAGE_COUNT + 1;
    CHECK(!exodus_page_integrity_store_append_adaptive(&store, pages[0]));
    CHECK(store.pages.page_count == PAGE_COUNT && store.pages.payload_used == PAYLOAD_BYTES);
    CHECK(memcmp(payload, payload_before, sizeof(payload)) == 0);
    CHECK(memcmp(ends, ends_before, sizeof(ends)) == 0);
    CHECK(memcmp(checksums, checksums_before, sizeof(checksums)) == 0);

    uint8_t restored[EXODUS_PAGE_BYTES];
    memset(restored, 0x6a, sizeof(restored));
    CHECK(!exodus_page_integrity_store_restore(NULL, 0, restored));
    CHECK(!exodus_page_integrity_store_restore(&store, PAGE_COUNT, restored));
    CHECK(!exodus_page_integrity_store_restore(&store, 0, NULL));
    CHECK(!exodus_page_integrity_store_append(NULL, pages[0]));
    CHECK(!exodus_page_integrity_store_append(&store, NULL));
    check_filled(restored, sizeof(restored), 0x6a);
}

enum { REPLACEMENT_CAPACITY = 3 * EXODUS_PAGE_BYTES };

struct replacement_fixture {
    uint8_t payload[REPLACEMENT_CAPACITY + 2];
    size_t ends[PAGE_COUNT + 2];
    uint32_t checksums[PAGE_COUNT + 2];
    struct exodus_page_integrity_store store;
};

typedef bool (*replace_function)(struct exodus_page_integrity_store *, size_t,
                                 const uint8_t *);

static const replace_function replacers[] = {
    exodus_page_integrity_store_replace,
    exodus_page_integrity_store_replace_adaptive,
};

static void prepare_replacement(struct replacement_fixture *fixture,
                                uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES])
{
    memset(fixture, 0xa3, sizeof(*fixture));
    for (size_t index = 0; index < PAGE_COUNT + 2; ++index)
        fixture->ends[index] = SIZE_MAX;
    fixture->store = make_store(fixture->payload + 1, REPLACEMENT_CAPACITY,
                                fixture->ends + 1, PAGE_COUNT,
                                fixture->checksums + 1, PAGE_COUNT);
    append_fixture(&fixture->store, pages);
}

static void check_replacement_contents(
    const struct replacement_fixture *fixture,
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES], const size_t expected[PAGE_COUNT],
    const size_t lengths[PAGE_COUNT], const uint32_t original_checksums[PAGE_COUNT])
{
    CHECK(fixture->store.pages.page_count == PAGE_COUNT);
    size_t used = 0;
    for (size_t index = 0; index < PAGE_COUNT; ++index) {
        uint8_t restored[EXODUS_PAGE_BYTES + 2];
        memset(restored, 0xc5, sizeof(restored));
        CHECK(exodus_page_integrity_store_restore(&fixture->store, index, restored + 1));
        CHECK(memcmp(restored + 1, pages[expected[index]], EXODUS_PAGE_BYTES) == 0);
        CHECK(restored[0] == 0xc5 && restored[sizeof(restored) - 1] == 0xc5);
        used += lengths[index];
        CHECK(fixture->store.pages.page_ends[index] == used);
        CHECK(fixture->store.checksums[index] == original_checksums[expected[index]]);
    }
    CHECK(fixture->store.pages.payload_used == used);
    CHECK(fixture->payload[0] == 0xa3);
    CHECK(fixture->payload[sizeof(fixture->payload) - 1] == 0xa3);
    CHECK(fixture->ends[0] == SIZE_MAX && fixture->ends[PAGE_COUNT + 1] == SIZE_MAX);
    CHECK(fixture->checksums[0] == UINT32_C(0xa3a3a3a3));
    CHECK(fixture->checksums[PAGE_COUNT + 1] == UINT32_C(0xa3a3a3a3));
}

static void check_replacement_round_trips(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    const size_t encoded_lengths[][PAGE_COUNT] = {
        {8, 72, EXODUS_PAGE_BYTES, EXODUS_PAGE_BYTES},
        {8, 72, WORD_RUN_BYTES, EXODUS_PAGE_BYTES},
    };
    const size_t operations[][2] = {
        {0, 3}, {3, 0}, {1, 2}, {2, 1}, {0, 2}, {1, 1}, {0, 0},
    };
    for (size_t variant = 0; variant < sizeof(replacers) / sizeof(replacers[0]); ++variant) {
        struct replacement_fixture fixture;
        prepare_replacement(&fixture, pages);
        uint32_t original_checksums[PAGE_COUNT];
        memcpy(original_checksums, fixture.store.checksums, sizeof(original_checksums));
        size_t expected[] = {0, 1, 2, 3};
        size_t lengths[] = {8, 72, WORD_RUN_BYTES, EXODUS_PAGE_BYTES};
        for (size_t operation = 0; operation < sizeof(operations) / sizeof(operations[0]); ++operation) {
            size_t index = operations[operation][0];
            size_t source = operations[operation][1];
            size_t old_used = fixture.store.pages.payload_used;
            size_t next_used = old_used - lengths[index] + encoded_lengths[variant][source];
            /* Growth must fit at the exact byte boundary; shrinking starts
             * with a valid, completely full store. */
            fixture.store.pages.payload_capacity = old_used > next_used ? old_used : next_used;
            CHECK(replacers[variant](&fixture.store, index, pages[source]));
            expected[index] = source;
            lengths[index] = encoded_lengths[variant][source];
            check_replacement_contents(&fixture, pages, expected, lengths, original_checksums);
        }
    }
}

static void expect_replacement_rejected(struct replacement_fixture *fixture,
                                        replace_function replace, size_t index,
                                        const uint8_t *page)
{
    struct replacement_fixture before;
    memcpy(&before, fixture, sizeof(before));
    CHECK(!replace(&fixture->store, index, page));
    /* Include unused backing space, guards, descriptors and every sidecar slot. */
    CHECK(memcmp(fixture, &before, sizeof(before)) == 0);
}

static void check_replacement_failure_atomicity(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    for (size_t variant = 0; variant < sizeof(replacers) / sizeof(replacers[0]); ++variant) {
        replace_function replace = replacers[variant];
        struct replacement_fixture fixture;
        prepare_replacement(&fixture, pages);
        CHECK(!replace(NULL, 0, pages[0]));
        expect_replacement_rejected(&fixture, replace, 0, NULL);
        expect_replacement_rejected(&fixture, replace, PAGE_COUNT, pages[0]);
        expect_replacement_rejected(&fixture, replace, SIZE_MAX, pages[0]);

        fixture.store.pages.payload_capacity = PAYLOAD_BYTES;
        expect_replacement_rejected(&fixture, replace, 0, pages[2]);
        expect_replacement_rejected(&fixture, replace, 0, pages[3]);
        fixture.store.pages.payload_capacity = PAYLOAD_BYTES - 8 + EXODUS_PAGE_BYTES - 1;
        expect_replacement_rejected(&fixture, replace, 0, pages[3]);

        prepare_replacement(&fixture, pages);
        fixture.store.checksum_capacity = PAGE_COUNT - 1;
        expect_replacement_rejected(&fixture, replace, 0, pages[0]);
        prepare_replacement(&fixture, pages);
        fixture.store.pages.page_count = PAGE_COUNT + 1;
        expect_replacement_rejected(&fixture, replace, 0, pages[0]);
        prepare_replacement(&fixture, pages);
        fixture.store.pages.payload_used = REPLACEMENT_CAPACITY + 1;
        expect_replacement_rejected(&fixture, replace, 0, pages[0]);
        prepare_replacement(&fixture, pages);
        fixture.store.pages.page_ends[1] = fixture.store.pages.page_ends[0];
        expect_replacement_rejected(&fixture, replace, 0, pages[0]);
        prepare_replacement(&fixture, pages);
        --fixture.store.pages.page_ends[PAGE_COUNT - 1];
        expect_replacement_rejected(&fixture, replace, 0, pages[0]);

        /* Monotonic offsets alone are insufficient: let the base store reject
         * a record larger than a page, without updating the integrity sidecar. */
        prepare_replacement(&fixture, pages);
        for (size_t index = 0; index < PAGE_COUNT; ++index)
            fixture.store.pages.page_ends[index] = EXODUS_PAGE_BYTES + 1 + index;
        fixture.store.pages.payload_used = EXODUS_PAGE_BYTES + PAGE_COUNT;
        expect_replacement_rejected(&fixture, replace, 0, pages[0]);
    }
}

static void check_replacement_payload_aliases(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    for (size_t variant = 0; variant < sizeof(replacers) / sizeof(replacers[0]); ++variant) {
        struct replacement_fixture fixture;
        prepare_replacement(&fixture, pages);
        uint32_t original_checksums[PAGE_COUNT];
        memcpy(original_checksums, fixture.store.checksums, sizeof(original_checksums));
        const uint8_t *source = fixture.store.pages.payload + fixture.store.pages.page_ends[2];
        CHECK(replacers[variant](&fixture.store, 0, source));
        const size_t expected[] = {3, 1, 2, 3};
        const size_t lengths[] = {EXODUS_PAGE_BYTES, 72, WORD_RUN_BYTES, EXODUS_PAGE_BYTES};
        check_replacement_contents(&fixture, pages, expected, lengths, original_checksums);

        /* A raw page may also replace itself without first copying it out. */
        CHECK(replacers[variant](&fixture.store, 0, fixture.store.pages.payload));
        check_replacement_contents(&fixture, pages, expected, lengths, original_checksums);
    }
}

static void check_replacement_retains_corruption_checks(void)
{
    uint8_t pages[PAGE_COUNT][EXODUS_PAGE_BYTES];
    fill_pages(pages);
    for (size_t variant = 0; variant < sizeof(replacers) / sizeof(replacers[0]); ++variant) {
        struct replacement_fixture fixture;
        prepare_replacement(&fixture, pages);
        uint32_t sparse_checksum = fixture.store.checksums[1];
        fixture.store.pages.payload[fixture.store.pages.page_ends[0] + 8 + 17] ^= 1;
        CHECK(replacers[variant](&fixture.store, 0, pages[3]));
        CHECK(fixture.store.checksums[1] == sparse_checksum);
        uint8_t restored[EXODUS_PAGE_BYTES];
        memset(restored, 0x6a, sizeof(restored));
        CHECK(!exodus_page_integrity_store_restore(&fixture.store, 1, restored));
        check_filled(restored, sizeof(restored), 0x6a);

        /* Deliberate write-back may replace a damaged page; corruption in
         * another record or its sidecar must remain detectable. */
        CHECK(replacers[variant](&fixture.store, 1, pages[1]));
        CHECK(exodus_page_integrity_store_restore(&fixture.store, 1, restored));
        CHECK(memcmp(restored, pages[1], sizeof(restored)) == 0);
        fixture.store.checksums[2] ^= 1;
        uint32_t damaged_checksum = fixture.store.checksums[2];
        CHECK(replacers[variant](&fixture.store, 0, pages[0]));
        CHECK(fixture.store.checksums[2] == damaged_checksum);
        memset(restored, 0x6a, sizeof(restored));
        CHECK(!exodus_page_integrity_store_restore(&fixture.store, 2, restored));
        check_filled(restored, sizeof(restored), 0x6a);
    }
}

int main(void)
{
    check_round_trip_and_guards();
    check_corruption_rejected_before_output();
    check_failure_atomicity();
    check_replacement_round_trips();
    check_replacement_failure_atomicity();
    check_replacement_payload_aliases();
    check_replacement_retains_corruption_checks();
    puts("Page integrity store tests passed");
    return EXIT_SUCCESS;
}
