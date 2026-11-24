#include "page_dedup_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum { UNIQUE_CAPACITY = 4, LOGICAL_CAPACITY = 8, PAYLOAD_CAPACITY = 4 * EXODUS_PAGE_BYTES };

struct fixture {
    uint8_t payload[PAYLOAD_CAPACITY + 2];
    size_t ends[UNIQUE_CAPACITY + 2];
    uint64_t hashes[UNIQUE_CAPACITY + 2];
    size_t refs[LOGICAL_CAPACITY + 2];
    struct exodus_page_dedup_store store;
};

static void init_fixture(struct fixture *fixture, size_t bytes, size_t unique, size_t logical)
{
    CHECK(bytes <= PAYLOAD_CAPACITY && unique <= UNIQUE_CAPACITY && logical <= LOGICAL_CAPACITY);
    memset(fixture->payload, 0xa3, sizeof(fixture->payload));
    for (size_t i = 0; i < UNIQUE_CAPACITY + 2; ++i) {
        fixture->ends[i] = SIZE_MAX;
        fixture->hashes[i] = UINT64_MAX;
    }
    for (size_t i = 0; i < LOGICAL_CAPACITY + 2; ++i)
        fixture->refs[i] = SIZE_MAX;
    fixture->store = (struct exodus_page_dedup_store){
        .unique_pages = {
            .payload = fixture->payload + 1,
            .payload_capacity = bytes,
            .page_ends = fixture->ends + 1,
            .page_capacity = unique
        },
        .fingerprints = fixture->hashes + 1,
        .page_refs = fixture->refs + 1,
        .page_capacity = logical
    };
}

static void dense_page(uint8_t *page, uint8_t variant)
{
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i)
        page[i] = (uint8_t)i;
    page[EXODUS_PAGE_BYTES - 1] ^= variant;
}

static void check_unchanged(const struct fixture *fixture, const struct fixture *before)
{
    CHECK(memcmp(fixture->payload, before->payload, sizeof(fixture->payload)) == 0);
    CHECK(memcmp(fixture->ends, before->ends, sizeof(fixture->ends)) == 0);
    CHECK(memcmp(fixture->hashes, before->hashes, sizeof(fixture->hashes)) == 0);
    CHECK(memcmp(fixture->refs, before->refs, sizeof(fixture->refs)) == 0);
    CHECK(fixture->store.page_count == before->store.page_count);
    CHECK(fixture->store.unique_pages.page_count == before->store.unique_pages.page_count);
    CHECK(fixture->store.unique_pages.payload_used == before->store.unique_pages.payload_used);
}

static void check_guards(const struct fixture *fixture)
{
    const struct exodus_page_dedup_store *store = &fixture->store;
    CHECK(fixture->payload[0] == 0xa3);
    CHECK(fixture->payload[store->unique_pages.payload_capacity + 1] == 0xa3);
    CHECK(fixture->ends[0] == SIZE_MAX && fixture->ends[store->unique_pages.page_capacity + 1] == SIZE_MAX);
    CHECK(fixture->hashes[0] == UINT64_MAX && fixture->hashes[store->unique_pages.page_capacity + 1] == UINT64_MAX);
    CHECK(fixture->refs[0] == SIZE_MAX && fixture->refs[store->page_capacity + 1] == SIZE_MAX);
}

static void check_two_dense_pages_share_one_payload(void)
{
    uint8_t payload[EXODUS_PAGE_BYTES], original[EXODUS_PAGE_BYTES], input[EXODUS_PAGE_BYTES];
    size_t end = SIZE_MAX, refs[2] = {SIZE_MAX, SIZE_MAX};
    uint64_t hash = 0;
    struct exodus_page_dedup_store store = {
        .unique_pages = {
            .payload = payload,
            .payload_capacity = sizeof(payload),
            .page_ends = &end,
            .page_capacity = 1
        },
        .fingerprints = &hash,
        .page_refs = refs,
        .page_capacity = 2
    };
    dense_page(original, 0);
    memcpy(input, original, sizeof(input));
    CHECK(exodus_page_dedup_store_append(&store, input));
    memset(input, 0, sizeof(input));
    CHECK(exodus_page_dedup_store_append(&store, original));
    CHECK(store.page_count == 2 && store.unique_pages.page_count == 1);
    CHECK(store.unique_pages.payload_used == EXODUS_PAGE_BYTES && end == EXODUS_PAGE_BYTES);
    CHECK(refs[0] == 0 && refs[1] == 0);
    for (size_t i = 0; i < store.page_count; ++i) {
        CHECK(exodus_page_dedup_store_restore(&store, i, input));
        CHECK(memcmp(input, original, sizeof(input)) == 0);
        memset(input, 0, sizeof(input));
    }
    CHECK(!exodus_page_dedup_store_append(&store, original));
    CHECK(store.page_count == 2 && store.unique_pages.page_count == 1);
}

static void check_mixed_pages_and_stable_indices(void)
{
    struct fixture fixture;
    init_fixture(&fixture, EXODUS_PAGE_BYTES + 8 + 72, 3, 6);
    uint8_t pages[3][EXODUS_PAGE_BYTES] = {{0}};
    dense_page(pages[0], 0);
    for (size_t i = 0; i < 64; ++i)
        pages[2][i] = (uint8_t)(i + 1);
    const size_t order[] = {0, 1, 0, 2, 1, 2};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i)
        CHECK(exodus_page_dedup_store_append(&fixture.store, pages[order[i]]));
    CHECK(fixture.store.unique_pages.page_count == 3 && fixture.store.page_count == 6);
    CHECK(fixture.store.unique_pages.payload_used == EXODUS_PAGE_BYTES + 8 + 72);
    CHECK(fixture.store.unique_pages.page_ends[0] == EXODUS_PAGE_BYTES);
    CHECK(fixture.store.unique_pages.page_ends[1] == EXODUS_PAGE_BYTES + 8);
    CHECK(fixture.store.unique_pages.page_ends[2] == EXODUS_PAGE_BYTES + 8 + 72);
    CHECK(memcmp(fixture.store.page_refs, order, sizeof(order)) == 0);

    uint8_t restored[EXODUS_PAGE_BYTES + 2];
    memset(restored, 0xc5, sizeof(restored));
    for (size_t i = fixture.store.page_count; i > 0; --i) {
        CHECK(exodus_page_dedup_store_restore(&fixture.store, i - 1, restored + 1));
        CHECK(memcmp(restored + 1, pages[order[i - 1]], EXODUS_PAGE_BYTES) == 0);
        CHECK(restored[0] == 0xc5 && restored[sizeof(restored) - 1] == 0xc5);
    }
    check_guards(&fixture);
}

static void check_fingerprint_collision_never_aliases_different_content(void)
{
    struct fixture fixture, reference;
    init_fixture(&fixture, 2 * EXODUS_PAGE_BYTES, 2, 3);
    init_fixture(&reference, EXODUS_PAGE_BYTES, 1, 1);
    uint8_t first[EXODUS_PAGE_BYTES], second[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    dense_page(first, 0);
    dense_page(second, 1);
    CHECK(exodus_page_dedup_store_append(&fixture.store, first));
    CHECK(exodus_page_dedup_store_append(&reference.store, second));
    /* Force the lookup-collision path without relying on a particular hash algorithm. */
    fixture.store.fingerprints[0] = reference.store.fingerprints[0];
    CHECK(exodus_page_dedup_store_append(&fixture.store, second));
    CHECK(fixture.store.unique_pages.page_count == 2);
    CHECK(fixture.store.unique_pages.payload_used == 2 * EXODUS_PAGE_BYTES);
    CHECK(fixture.store.page_refs[0] == 0 && fixture.store.page_refs[1] == 1);
    CHECK(exodus_page_dedup_store_append(&fixture.store, second));
    CHECK(fixture.store.page_refs[2] == 1 && fixture.store.unique_pages.page_count == 2);
    CHECK(exodus_page_dedup_store_restore(&fixture.store, 0, restored));
    CHECK(memcmp(restored, first, sizeof(restored)) == 0);
    CHECK(exodus_page_dedup_store_restore(&fixture.store, 1, restored));
    CHECK(memcmp(restored, second, sizeof(restored)) == 0);
    check_guards(&fixture);
}

static void check_capacity_failures_are_atomic(void)
{
    uint8_t zero[EXODUS_PAGE_BYTES] = {0}, dense[EXODUS_PAGE_BYTES];
    dense_page(dense, 0);
    for (unsigned limit = 0; limit < 3; ++limit) {
        struct fixture fixture;
        init_fixture(&fixture, limit == 0 ? EXODUS_PAGE_BYTES + 7 : PAYLOAD_CAPACITY,
                     limit == 1 ? 1 : UNIQUE_CAPACITY, limit == 2 ? 1 : LOGICAL_CAPACITY);
        CHECK(exodus_page_dedup_store_append(&fixture.store, zero));
        struct fixture before = fixture;
        CHECK(!exodus_page_dedup_store_append(&fixture.store, dense));
        check_unchanged(&fixture, &before);
        if (limit == 2) {
            CHECK(!exodus_page_dedup_store_append(&fixture.store, zero));
            check_unchanged(&fixture, &before);
        } else {
            CHECK(exodus_page_dedup_store_append(&fixture.store, zero));
            CHECK(fixture.store.page_count == 2 && fixture.store.unique_pages.page_count == 1);
            CHECK(fixture.store.unique_pages.payload_used == 8);
        }
        check_guards(&fixture);
    }
}

static void check_corruption_is_rejected(void)
{
    struct fixture fixture;
    init_fixture(&fixture, 16, 2, 3);
    uint8_t zero[EXODUS_PAGE_BYTES] = {0}, restored[EXODUS_PAGE_BYTES];
    CHECK(exodus_page_dedup_store_append(&fixture.store, zero));
    /* The sparse mask now requires a block outside this page's encoded length. */
    fixture.store.unique_pages.payload[0] = 1;
    struct fixture before = fixture;
    CHECK(!exodus_page_dedup_store_append(&fixture.store, zero));
    check_unchanged(&fixture, &before);
    memset(restored, 0xc5, sizeof(restored));
    CHECK(!exodus_page_dedup_store_restore(&fixture.store, 0, restored));
    fixture.store.unique_pages.payload[0] = 0;
    fixture.store.page_refs[0] = SIZE_MAX;
    CHECK(!exodus_page_dedup_store_restore(&fixture.store, 0, restored));
    CHECK(!exodus_page_dedup_store_restore(&fixture.store, fixture.store.page_count, restored));
    for (size_t i = 0; i < sizeof(restored); ++i)
        CHECK(restored[i] == 0xc5);
    check_guards(&fixture);
}

static void check_invalid_inputs(void)
{
    struct fixture fixture;
    init_fixture(&fixture, PAYLOAD_CAPACITY, UNIQUE_CAPACITY, LOGICAL_CAPACITY);
    uint8_t page[EXODUS_PAGE_BYTES] = {0};
    const struct exodus_page_dedup_store original = fixture.store;
    CHECK(!exodus_page_dedup_store_append(NULL, page));
    CHECK(!exodus_page_dedup_store_append(&fixture.store, NULL));
    CHECK(!exodus_page_dedup_store_restore(NULL, 0, page));
    CHECK(!exodus_page_dedup_store_restore(&fixture.store, 0, NULL));
    for (unsigned invalid = 0; invalid < 8; ++invalid) {
        fixture.store = original;
        switch (invalid) {
        case 0: fixture.store.fingerprints = NULL; break;
        case 1: fixture.store.page_refs = NULL; break;
        case 2: fixture.store.unique_pages.payload = NULL; break;
        case 3: fixture.store.unique_pages.page_ends = NULL; break;
        case 4: fixture.store.page_count = fixture.store.page_capacity + 1; break;
        case 5: fixture.store.unique_pages.page_count = fixture.store.unique_pages.page_capacity + 1; break;
        case 6: fixture.store.unique_pages.payload_used = fixture.store.unique_pages.payload_capacity + 1; break;
        case 7: fixture.store.unique_pages.page_count = 1; break;
        }
        const struct fixture before = fixture;
        CHECK(!exodus_page_dedup_store_append(&fixture.store, page));
        CHECK(!exodus_page_dedup_store_restore(&fixture.store, 0, page));
        check_unchanged(&fixture, &before);
    }
}

int main(void)
{
    check_two_dense_pages_share_one_payload();
    check_mixed_pages_and_stable_indices();
    check_fingerprint_collision_never_aliases_different_content();
    check_capacity_failures_are_atomic();
    check_corruption_is_rejected();
    check_invalid_inputs();
    puts("Immutable deduplicating page store tests passed");
    return 0;
}
