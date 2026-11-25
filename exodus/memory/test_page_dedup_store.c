#include "page_dedup_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum { UNIQUE_CAPACITY = 4, LOGICAL_CAPACITY = 8 };

struct fixture {
    uint8_t payload[UNIQUE_CAPACITY * EXODUS_PAGE_BYTES];
    size_t ends[UNIQUE_CAPACITY];
    uint64_t hashes[UNIQUE_CAPACITY];
    size_t refs[LOGICAL_CAPACITY];
    struct exodus_page_dedup_store store;
};

static void init_fixture(struct fixture *f)
{
    memset(f, 0xa3, sizeof(*f));
    f->store = (struct exodus_page_dedup_store) {
        .unique = {
            .payload = f->payload,
            .payload_capacity = sizeof(f->payload),
            .page_ends = f->ends,
            .page_capacity = UNIQUE_CAPACITY
        },
        .fingerprints = f->hashes,
        .page_refs = f->refs,
        .page_capacity = LOGICAL_CAPACITY
    };
}

static void check_filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    for (size_t i = 0; i < length; ++i)
        CHECK(bytes[i] == value);
}

static void check_append_failure_unchanged(struct fixture *f, const uint8_t *page)
{
    struct fixture before = *f;
    CHECK(!exodus_page_dedup_store_append(&f->store, page));
    CHECK(memcmp(f, &before, sizeof(*f)) == 0);
}

static void check_restore_failure_unchanged(const struct exodus_page_dedup_store *store,
                                           size_t index)
{
    uint8_t restored[EXODUS_PAGE_BYTES];
    memset(restored, 0xc5, sizeof(restored));
    CHECK(!exodus_page_dedup_store_restore(store, index, restored));
    check_filled(restored, sizeof(restored), 0xc5);
}

static void check_raw_sharing_and_memory_budget(void)
{
    uint8_t raw[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    uint8_t payload[EXODUS_PAGE_BYTES];
    size_t end[1], refs[2];
    uint64_t hashes[1];
    struct exodus_page_dedup_store shared = {
        .unique = { .payload = payload, .payload_capacity = sizeof(payload),
                    .page_ends = end, .page_capacity = 1 },
        .fingerprints = hashes, .page_refs = refs, .page_capacity = 2
    };
    uint8_t baseline_payload[2 * EXODUS_PAGE_BYTES];
    size_t baseline_ends[2];
    struct exodus_page_store baseline = {
        .payload = baseline_payload, .payload_capacity = sizeof(baseline_payload),
        .page_ends = baseline_ends, .page_capacity = 2
    };

    memset(raw, 0x5a, sizeof(raw));
    for (size_t i = 0; i < 2; ++i) {
        CHECK(exodus_page_store_append(&baseline, raw));
        CHECK(exodus_page_dedup_store_append(&shared, raw));
    }
    CHECK(baseline.payload_used == 2 * EXODUS_PAGE_BYTES);
    CHECK(shared.unique.payload_used == EXODUS_PAGE_BYTES);
    CHECK(shared.unique.page_count == 1 && shared.page_count == 2);
    CHECK(refs[0] == 0 && refs[1] == 0 && end[0] == EXODUS_PAGE_BYTES);

    /* Source reuse and edits of a restored page never mutate shared backing. */
    memset(raw, 0x17, sizeof(raw));
    for (size_t i = 0; i < 2; ++i) {
        CHECK(exodus_page_dedup_store_restore(&shared, i, restored));
        check_filled(restored, sizeof(restored), 0x5a);
        memset(restored, 0x38, sizeof(restored));
    }
    CHECK(exodus_page_dedup_store_restore(&shared, 0, restored));
    check_filled(restored, sizeof(restored), 0x5a);

    const size_t shared_reserved = sizeof(shared) + sizeof(payload) + sizeof(end) +
                                   sizeof(hashes) + sizeof(refs);
    const size_t baseline_reserved = sizeof(baseline) + sizeof(baseline_payload) +
                                     sizeof(baseline_ends);
    CHECK(shared_reserved < baseline_reserved);
    printf("Two equal raw pages: payload %zu -> %zu bytes; reserved backing + metadata "
           "+ descriptor %zu -> %zu bytes (caller staging excluded)\n",
           baseline.payload_used, shared.unique.payload_used,
           baseline_reserved, shared_reserved);
}

static void check_mixed_pages_and_full_physical_capacity(void)
{
    struct fixture f;
    init_fixture(&f);
    uint8_t pages[3][EXODUS_PAGE_BYTES] = {{0}};
    pages[1][EXODUS_PAGE_BYTES - 1] = 0x9a;
    memset(pages[2], 0x5b, sizeof(pages[2]));
    f.store.unique.payload_capacity = 8 + 72 + EXODUS_PAGE_BYTES;
    f.store.unique.page_capacity = 3;

    for (size_t i = 0; i < 6; ++i) {
        CHECK(exodus_page_dedup_store_append(&f.store, pages[i % 3]));
        CHECK(f.refs[i] == i % 3);
    }
    CHECK(f.store.unique.page_count == 3 && f.store.page_count == 6);
    CHECK(f.store.unique.payload_used == f.store.unique.payload_capacity);
    check_filled(f.payload + f.store.unique.payload_used,
                 sizeof(f.payload) - f.store.unique.payload_used, 0xa3);
    uint8_t restored[EXODUS_PAGE_BYTES];
    for (size_t i = 0; i < 6; ++i) {
        CHECK(exodus_page_dedup_store_restore(&f.store, i, restored));
        CHECK(memcmp(restored, pages[i % 3], sizeof(restored)) == 0);
    }
}

static void check_capacity_failures(void)
{
    struct fixture f;
    uint8_t zero[EXODUS_PAGE_BYTES] = {0}, raw[EXODUS_PAGE_BYTES];
    memset(raw, 0x5a, sizeof(raw));

    init_fixture(&f);
    f.store.page_capacity = 1;
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    check_append_failure_unchanged(&f, zero);
    check_append_failure_unchanged(&f, raw);

    init_fixture(&f);
    f.store.unique.payload_capacity = 8;
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    check_append_failure_unchanged(&f, raw);
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    CHECK(f.store.page_count == 2 && f.store.unique.page_count == 1);

    init_fixture(&f);
    f.store.unique.page_capacity = 1;
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    check_append_failure_unchanged(&f, raw);
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    CHECK(f.store.page_count == 2 && f.store.unique.page_count == 1);
}

static void check_fingerprint_collision(void)
{
    struct fixture f, other;
    init_fixture(&f);
    init_fixture(&other);
    uint8_t a[EXODUS_PAGE_BYTES], b[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    memset(a, 0x31, sizeof(a));
    memset(b, 0x72, sizeof(b));
    CHECK(exodus_page_dedup_store_append(&f.store, a));
    CHECK(exodus_page_dedup_store_append(&other.store, b));
    /* Force a candidate collision using a fingerprint obtained through the API.
     * Test-only fault injection; applications must not change this metadata. */
    f.hashes[0] = other.hashes[0];
    CHECK(exodus_page_dedup_store_append(&f.store, b));
    CHECK(f.store.unique.page_count == 2 && f.refs[1] == 1);
    CHECK(exodus_page_dedup_store_append(&f.store, b));
    CHECK(f.store.unique.page_count == 2 && f.refs[2] == 1);
    CHECK(exodus_page_dedup_store_restore(&f.store, 0, restored));
    CHECK(memcmp(restored, a, sizeof(restored)) == 0);
    for (size_t i = 1; i < 3; ++i) {
        CHECK(exodus_page_dedup_store_restore(&f.store, i, restored));
        CHECK(memcmp(restored, b, sizeof(restored)) == 0);
    }
}

static void check_invalid_metadata_and_encoding(void)
{
    struct fixture f;
    uint8_t zero[EXODUS_PAGE_BYTES] = {0};
    CHECK(!exodus_page_dedup_store_append(NULL, zero));
    check_restore_failure_unchanged(NULL, 0);

    for (size_t scenario = 0; scenario < 12; ++scenario) {
        init_fixture(&f);
        CHECK(exodus_page_dedup_store_append(&f.store, zero));
        switch (scenario) {
            case 0: f.store.unique.payload = NULL; break;
            case 1: f.store.unique.page_ends = NULL; break;
            case 2: f.store.fingerprints = NULL; break;
            case 3: f.store.page_refs = NULL; break;
            case 4: f.store.page_count = f.store.page_capacity + 1; break;
            case 5: f.store.unique.page_count = f.store.unique.page_capacity + 1; break;
            case 6: f.store.unique.payload_used = f.store.unique.payload_capacity + 1; break;
            case 7: f.store.page_count = 0; break;
            case 8: f.store.unique.page_count = 0; break;
            case 9: f.store.unique.payload_used = 0; break;
            case 10: f.ends[0] = 7; break;
            case 11: f.store.unique.page_count = 0; f.store.unique.payload_used = 0; break;
        }
        check_append_failure_unchanged(&f, zero);
        check_restore_failure_unchanged(&f.store, 0);
    }

    init_fixture(&f);
    check_append_failure_unchanged(&f, NULL);
    CHECK(!exodus_page_dedup_store_restore(&f.store, 0, NULL));
    check_restore_failure_unchanged(&f.store, 0);
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    check_restore_failure_unchanged(&f.store, f.store.page_count);
    check_restore_failure_unchanged(&f.store, SIZE_MAX);
    f.refs[0] = SIZE_MAX;
    check_restore_failure_unchanged(&f.store, 0);

    init_fixture(&f);
    CHECK(exodus_page_dedup_store_append(&f.store, zero));
    f.payload[0] = 1; /* Eight-byte sparse page now claims a missing data block. */
    check_append_failure_unchanged(&f, zero);
    check_restore_failure_unchanged(&f.store, 0);
}

static void check_many_logical_pages(void)
{
    enum { UNIQUE = 16, LOGICAL = 96 };
    uint8_t originals[UNIQUE][EXODUS_PAGE_BYTES] = {{0}};
    uint8_t payload[UNIQUE * EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    size_t ends[UNIQUE], refs[LOGICAL], expected[LOGICAL];
    uint64_t hashes[UNIQUE];
    struct exodus_page_dedup_store store = {
        .unique = { .payload = payload, .payload_capacity = sizeof(payload),
                    .page_ends = ends, .page_capacity = UNIQUE },
        .fingerprints = hashes, .page_refs = refs, .page_capacity = LOGICAL
    };
    uint32_t random = 2839;
    for (size_t i = 1; i < UNIQUE; ++i) {
        if (i < UNIQUE / 2) {
            originals[i][i * 64] = (uint8_t)i;
        } else {
            for (size_t j = 0; j < EXODUS_PAGE_BYTES; ++j) {
                random = random * UINT32_C(1664525) + UINT32_C(1013904223);
                originals[i][j] = (uint8_t)(random >> 24) | 1;
            }
        }
    }
    for (size_t i = 0; i < LOGICAL; ++i) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        expected[i] = i < UNIQUE ? i : random % UNIQUE;
        CHECK(exodus_page_dedup_store_append(&store, originals[expected[i]]));
        CHECK(store.page_count == i + 1);
    }
    CHECK(store.unique.page_count == UNIQUE);
    CHECK(store.unique.payload_used == 8 + 7 * 72 + 8 * EXODUS_PAGE_BYTES);
    for (size_t i = 0; i < LOGICAL; ++i) {
        CHECK(exodus_page_dedup_store_restore(&store, i, restored));
        CHECK(memcmp(restored, originals[expected[i]], sizeof(restored)) == 0);
    }
}

static void check_adaptive_sharing(void)
{
    struct fixture f;
    init_fixture(&f);
    uint8_t pages[4][EXODUS_PAGE_BYTES] = {{0}};
    uint8_t restored[EXODUS_PAGE_BYTES];
    /* 8- and 16-byte tiles, neither reducible to four-byte word fills. */
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i) {
        pages[1][i] = (uint8_t)(1 + i % 8);
        pages[2][i] = (uint8_t)(17 + i % 16);
        pages[3][i] = (uint8_t)(33 + i % 8);
    }
    f.store.unique.payload_capacity = 8 + 9 + 17;
    f.store.unique.page_capacity = 3;
    CHECK(exodus_page_dedup_store_append(&f.store, pages[0]));
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store, pages[1]));
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store, pages[2]));
    CHECK(f.store.unique.payload_used == 34);
    CHECK(f.ends[0] == 8 && f.ends[1] == 17 && f.ends[2] == 34);
    /* The legacy entry point can share adaptive pages and vice versa. */
    CHECK(exodus_page_dedup_store_append(&f.store, pages[1]));
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store, pages[0]));
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store, pages[2]));
    const size_t expected[] = {0, 1, 2, 1, 0, 2};
    CHECK(f.store.page_count == 6 && f.store.unique.page_count == 3);
    CHECK(f.store.unique.payload_used == 34);
    for (size_t i = 0; i < 6; ++i) {
        CHECK(f.refs[i] == expected[i]);
        CHECK(exodus_page_dedup_store_restore(&f.store, i, restored));
        CHECK(memcmp(restored, pages[expected[i]], sizeof(restored)) == 0);
    }
    struct fixture before = f;
    CHECK(!exodus_page_dedup_store_append_adaptive(&f.store, pages[3]));
    CHECK(memcmp(&f, &before, sizeof(f)) == 0);
    /* A corrupt adaptive candidate cannot be treated as a duplicate. */
    f.payload[8] = 0xff;
    before = f;
    CHECK(!exodus_page_dedup_store_append_adaptive(&f.store, pages[1]));
    CHECK(memcmp(&f, &before, sizeof(f)) == 0);
    check_restore_failure_unchanged(&f.store, 1);
}

static void check_corrupt_physical_index_rejects_append(void)
{
    struct fixture f;
    uint8_t pages[4][EXODUS_PAGE_BYTES] = {{0}};
    pages[1][0] = 1;
    pages[2][0] = 2;
    pages[3][0] = 3;
    for (size_t scenario = 0; scenario < 4; ++scenario) {
        init_fixture(&f);
        for (size_t i = 0; i < 3; ++i)
            CHECK(exodus_page_dedup_store_append(&f.store, pages[i]));
        switch (scenario) {
        case 0: f.ends[0] = 0; break;
        case 1: f.ends[1] = f.ends[0]; break;
        case 2: f.ends[1] = f.store.unique.payload_used + 1; break;
        case 3: f.ends[0] = EXODUS_PAGE_BYTES + 1; break;
        }
        /* Distinct input skips all hash candidates; zero can match early. */
        for (size_t candidate = 0; candidate < 4; candidate += 3) {
            struct fixture before;
            memcpy(&before, &f, sizeof(f));
            CHECK(!exodus_page_dedup_store_append(&f.store, pages[candidate]));
            CHECK(memcmp(&f, &before, sizeof(f)) == 0);
            CHECK(!exodus_page_dedup_store_append_adaptive(&f.store, pages[candidate]));
            CHECK(memcmp(&f, &before, sizeof(f)) == 0);
        }
    }
}

/* Equal hashes and a 4095-byte common prefix must still distinguish raw
 * pages, including when replacement subsequently shares an equal page. */
static void check_raw_tail_collision_and_replace(void)
{
    struct fixture f;
    init_fixture(&f);
    uint8_t a[EXODUS_PAGE_BYTES], b[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    memset(a, 0x31, sizeof a);
    memcpy(b, a, sizeof b);
    b[EXODUS_PAGE_BYTES - 1] = 0x72;
    CHECK(exodus_page_dedup_store_append(&f.store, a));
    CHECK(f.ends[0] == EXODUS_PAGE_BYTES);
    f.hashes[0] = exodus_page_fingerprint(b); /* test-only collision */
    CHECK(exodus_page_dedup_store_append(&f.store, b));
    CHECK(f.store.unique.page_count == 2 && f.ends[1] == 2 * EXODUS_PAGE_BYTES);
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store, b));
    CHECK(f.store.unique.page_count == 2 && f.refs[2] == 1);
    CHECK(exodus_page_dedup_store_restore(&f.store, 0, restored));
    CHECK(memcmp(restored, a, sizeof a) == 0);
    CHECK(exodus_page_dedup_store_replace_adaptive(&f.store, 0, b));
    CHECK(f.store.unique.page_count == 1 && f.store.page_count == 3);
    for(size_t i=0;i<3;i++){
        CHECK(f.refs[i] == 0);
        CHECK(exodus_page_dedup_store_restore(&f.store, i, restored));
        CHECK(memcmp(restored, b, sizeof b) == 0);
    }
}

/* BDI candidates with equal hashes must be compared logically, including
 * mixed RAW/BDI blocks and invalid layouts. */
static void check_bdi_compare_collision_and_corruption(void)
{
    struct fixture f;
    init_fixture(&f);
    uint8_t a[EXODUS_PAGE_BYTES], b[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    for(size_t w=0;w<EXODUS_PAGE_BYTES/4;w++){
        a[w*4]=(uint8_t)(w%16);
        a[w*4+1]=0;
        a[w*4+2]=0x34;
        a[w*4+3]=0x12;
    }
    memcpy(b,a,sizeof b);
    b[sizeof b-1]^=1;
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store,a));
    CHECK(f.ends[0]==1289 && f.payload[0]==EXODUS_PAGE_BDI_VERSION);
    f.hashes[0]=exodus_page_fingerprint(b);
    CHECK(exodus_page_dedup_store_append_adaptive(&f.store,b));
    CHECK(f.ends[1]-f.ends[0]==1333);
    CHECK(f.payload[f.ends[0]]==EXODUS_PAGE_BDI_VERSION);
    CHECK(exodus_page_dedup_store_append(&f.store,b));
    CHECK(f.store.unique.page_count==2 && f.refs[2]==1);
    CHECK(exodus_page_dedup_store_restore(&f.store,0,restored));
    CHECK(memcmp(a,restored,sizeof a)==0);
    CHECK(exodus_page_dedup_store_replace_adaptive(&f.store,0,b));
    CHECK(f.store.unique.page_count==1 && f.store.page_count==3);
    for(size_t i=0;i<3;i++){
        CHECK(f.refs[i]==0);
        CHECK(exodus_page_dedup_store_restore(&f.store,i,restored));
        CHECK(memcmp(b,restored,sizeof b)==0);
    }
    /* Corrupt a mask bit without resizing the backing. A mismatch must
     * not turn invalid backing into an accepted independent record. */
    f.payload[1]^=1;
    struct fixture before;
    memcpy(&before,&f,sizeof f);
    CHECK(!exodus_page_dedup_store_append_adaptive(&f.store,b));
    CHECK(memcmp(&before,&f,sizeof f)==0);
    CHECK(!exodus_page_dedup_store_replace_adaptive(&f.store,0,a));
    CHECK(memcmp(&before,&f,sizeof f)==0);
    check_restore_failure_unchanged(&f.store,0);
}

/* A prospective hot edit must reject the first hash collision and share
 * the later exact candidate without changing an alias of the source.
 * Exercise both hot-to-hot and hot-to-cold comparison paths. */
static void check_hot_prospective_collision(void)
{
    enum { SLOTS=256 };
    for (unsigned hot_candidates=0; hot_candidates<2; ++hot_candidates) {
        struct fixture f;
        init_fixture(&f);
        size_t starts[UNIQUE_CAPACITY]={0};
        uint32_t order[UNIQUE_CAPACITY]={0};
        uint8_t small[SLOTS*20], raw[SLOTS*64];
        uint16_t sr[SLOTS]={0}, rr[SLOTS]={0};
        struct exodus_bdi_free_index free_index;
        struct exodus_bdi_slab slab={
            .small=small,.raw=raw,.small_refs=sr,.raw_refs=rr,
            .small_capacity=SLOTS,.raw_capacity=SLOTS
        };
        struct exodus_hot_entry hot[UNIQUE_CAPACITY]={0};
        CHECK(exodus_bdi_slab_attach_index(&slab,&free_index));
        f.store.unique.page_starts=starts;
        f.store.unique.page_order=order;
        f.store.slab=&slab;
        f.store.hot=hot;
        f.store.hot_capacity=UNIQUE_CAPACITY;
        uint8_t a[EXODUS_PAGE_BYTES], b[EXODUS_PAGE_BYTES],
                collision[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
        memset(a,0x31,sizeof a);
        memcpy(b,a,sizeof b);
        memset(b,0x72,4);
        memcpy(collision,b,sizeof collision);
        collision[sizeof collision-1]^=1;
        CHECK(exodus_page_dedup_store_append_adaptive(&f.store,a));
        CHECK(exodus_page_dedup_store_append_adaptive(&f.store,collision));
        CHECK(exodus_page_dedup_store_append_adaptive(&f.store,b));
        CHECK(exodus_page_dedup_store_append_adaptive(&f.store,a));
        CHECK(f.refs[0]==0 && f.refs[1]==1 && f.refs[2]==2 && f.refs[3]==0);
        CHECK(exodus_page_dedup_store_promote(&f.store,0));
        if(hot_candidates) {
            CHECK(exodus_page_dedup_store_promote(&f.store,1));
            CHECK(exodus_page_dedup_store_promote(&f.store,2));
        }
        f.hashes[1]=f.hashes[2]; /* test-only collision */
        CHECK(exodus_page_dedup_store_write_word(&f.store,0,0,b));
        CHECK(f.store.unique.page_count==3 && f.store.page_count==4);
        CHECK(f.refs[0]==f.refs[2] && f.refs[0]!=f.refs[1] && f.refs[0]!=f.refs[3]);
        const uint8_t *expected[]={b,collision,b,a};
        for(size_t i=0;i<4;i++) {
            CHECK(exodus_page_dedup_store_restore(&f.store,i,restored));
            CHECK(!memcmp(restored,expected[i],sizeof restored));
        }
        CHECK(exodus_page_store_validate(&f.store.unique));
        CHECK(exodus_bdi_slab_check_index(&slab));
    }
}

int main(void)
{
    check_hot_prospective_collision();
    check_bdi_compare_collision_and_corruption();
    check_raw_tail_collision_and_replace();
    check_corrupt_physical_index_rejects_append();
    check_adaptive_sharing();
    check_raw_sharing_and_memory_budget();
    check_mixed_pages_and_full_physical_capacity();
    check_capacity_failures();
    check_fingerprint_collision();
    check_invalid_metadata_and_encoding();
    check_many_logical_pages();
    puts("Deduplicated cold page store tests passed");
    return 0;
}
