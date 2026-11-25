#include "page_dedup_store.h"

#include <string.h>

static bool valid_store(const struct exodus_page_dedup_store *store)
{
    return store && store->fingerprints && store->page_refs &&
        store->unique.payload && store->unique.page_ends &&
        store->page_count <= store->page_capacity &&
        store->unique.page_count <= store->unique.page_capacity &&
        store->unique.page_count <= store->page_count &&
        ((store->unique.page_count == 0) == (store->page_count == 0)) &&
        store->unique.payload_used <= store->unique.payload_capacity &&
        ((store->unique.page_count == 0) == (store->unique.payload_used == 0)) &&
        (store->unique.page_count == 0 ||
         store->unique.page_ends[store->unique.page_count - 1] == store->unique.payload_used);
}

static uint64_t page_fingerprint(const uint8_t *page)
{
    /* FNV-1a only filters candidates. Restored bytes decide equality. */
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i) {
        hash ^= page[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool append_page(struct exodus_page_dedup_store *store,
                        const uint8_t *page, bool adaptive)
{
    if (!page || !valid_store(store) || store->page_count == store->page_capacity)
        return false;

    uint64_t hash = page_fingerprint(page);
    size_t physical = store->unique.page_count;
    for (size_t i = 0; i < store->unique.page_count; ++i) {
        if (store->fingerprints[i] != hash)
            continue;
        uint8_t restored[EXODUS_PAGE_BYTES];
        if (!exodus_page_store_restore(&store->unique, i, restored))
            return false;
        if (memcmp(restored, page, sizeof(restored)) == 0) {
            physical = i;
            break;
        }
    }

    if (physical == store->unique.page_count) {
        if (!(adaptive ? exodus_page_store_append_adaptive(&store->unique, page)
                       : exodus_page_store_append(&store->unique, page)))
            return false;
        store->fingerprints[physical] = hash;
    }
    store->page_refs[store->page_count] = physical;
    ++store->page_count;
    return true;
}

bool exodus_page_dedup_store_append(struct exodus_page_dedup_store *store,
                                   const uint8_t *page)
{
    return append_page(store, page, false);
}

bool exodus_page_dedup_store_append_adaptive(struct exodus_page_dedup_store *store,
                                            const uint8_t *page)
{
    return append_page(store, page, true);
}

bool exodus_page_dedup_store_restore(const struct exodus_page_dedup_store *store,
                                    size_t index, uint8_t *page)
{
    if (!page || !valid_store(store) || index >= store->page_count)
        return false;
    return exodus_page_store_restore(&store->unique, store->page_refs[index], page);
}
