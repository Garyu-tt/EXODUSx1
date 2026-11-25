#include "page_store.h"

#include <string.h>

static bool append_page(struct exodus_page_store *store, const uint8_t *page, bool adaptive)
{
    if (!store || !page || !store->payload || !store->page_ends ||
        store->page_count >= store->page_capacity ||
        store->payload_used > store->payload_capacity)
        return false;

    uint8_t encoded[EXODUS_PAGE_BYTES];
    size_t size = adaptive ? exodus_page_encode_adaptive(page, encoded)
                           : exodus_page_encode(page, encoded);
    if (size > store->payload_capacity - store->payload_used)
        return false;

    memcpy(store->payload + store->payload_used, encoded, size);
    store->payload_used += size;
    store->page_ends[store->page_count] = store->payload_used;
    ++store->page_count;
    return true;
}

bool exodus_page_store_append(struct exodus_page_store *store, const uint8_t *page)
{
    return append_page(store, page, false);
}

bool exodus_page_store_append_adaptive(struct exodus_page_store *store, const uint8_t *page)
{
    return append_page(store, page, true);
}

bool exodus_page_store_restore(const struct exodus_page_store *store, size_t index, uint8_t *page)
{
    if (!store || !page || !store->payload || !store->page_ends ||
        index >= store->page_count || store->page_count > store->page_capacity ||
        store->payload_used > store->payload_capacity)
        return false;

    size_t start = index ? store->page_ends[index - 1] : 0;
    size_t end = store->page_ends[index];
    if (start >= end || end > store->payload_used)
        return false;

    return exodus_page_decode(store->payload + start, end - start, page);
}

bool exodus_page_store_replace(struct exodus_page_store *store, size_t index, const uint8_t *page)
{
    if (!store || !page || !store->payload || !store->page_ends ||
        index >= store->page_count || store->page_count > store->page_capacity ||
        store->payload_used > store->payload_capacity)
        return false;

    /* Every following offset will move with the tail. Validate the entire
     * index before touching either buffer. */
    size_t previous = 0;
    for (size_t i = 0; i < store->page_count; ++i) {
        size_t end = store->page_ends[i];
        if (end <= previous || end > store->payload_used || end - previous > EXODUS_PAGE_BYTES)
            return false;
        previous = end;
    }
    if (previous != store->payload_used)
        return false;

    size_t start = index ? store->page_ends[index - 1] : 0;
    size_t end = store->page_ends[index];
    size_t old_size = end - start;
    uint8_t encoded[EXODUS_PAGE_BYTES];
    size_t size = exodus_page_encode(page, encoded);
    size_t other_bytes = store->payload_used - old_size;
    if (size > store->payload_capacity - other_bytes)
        return false;

    memmove(store->payload + start + size, store->payload + end, store->payload_used - end);
    memcpy(store->payload + start, encoded, size);
    for (size_t i = index; i < store->page_count; ++i) {
        if (size >= old_size)
            store->page_ends[i] += size - old_size;
        else
            store->page_ends[i] -= old_size - size;
    }
    store->payload_used = other_bytes + size;
    return true;
}
