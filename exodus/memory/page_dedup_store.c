#include "page_dedup_store.h"

#include <string.h>

static bool valid_store(const struct exodus_page_dedup_store *store)
{
    if (!store || !store->fingerprints || !store->page_refs)
        return false;
    const struct exodus_page_store *unique = &store->unique_pages;
    return unique->payload && unique->page_ends &&
           store->page_count <= store->page_capacity &&
           unique->page_count <= unique->page_capacity &&
           unique->page_count <= store->page_count &&
           unique->payload_used <= unique->payload_capacity;
}

static uint64_t fingerprint(const uint8_t *page)
{
    /* FNV-1a is only a lookup filter, never evidence of page equality. */
    uint64_t value = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i) {
        value ^= page[i];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static bool find_page(const struct exodus_page_dedup_store *store, const uint8_t *page,
                      uint64_t hash, size_t *index)
{
    uint8_t restored[EXODUS_PAGE_BYTES];
    *index = store->unique_pages.page_count;
    for (size_t i = 0; i < store->unique_pages.page_count; ++i) {
        if (store->fingerprints[i] != hash)
            continue;
        if (!exodus_page_store_restore(&store->unique_pages, i, restored))
            return false;
        if (memcmp(page, restored, sizeof(restored)) == 0) {
            *index = i;
            break;
        }
    }
    return true;
}

bool exodus_page_dedup_store_append(struct exodus_page_dedup_store *store, const uint8_t *page)
{
    if (!page || !valid_store(store) || store->page_count == store->page_capacity)
        return false;

    const uint64_t hash = fingerprint(page);
    size_t index;
    if (!find_page(store, page, hash, &index))
        return false;
    if (index == store->unique_pages.page_count) {
        if (!exodus_page_store_append(&store->unique_pages, page))
            return false;
        store->fingerprints[index] = hash;
    }
    store->page_refs[store->page_count++] = index;
    return true;
}

bool exodus_page_dedup_store_restore(const struct exodus_page_dedup_store *store,
                                   size_t index, uint8_t *page)
{
    if (!page || !valid_store(store) || index >= store->page_count)
        return false;
    return exodus_page_store_restore(&store->unique_pages, store->page_refs[index], page);
}
