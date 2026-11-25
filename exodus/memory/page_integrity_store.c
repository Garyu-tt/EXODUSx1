#include "page_integrity_store.h"

#include <string.h>

static uint32_t page_crc32(const uint8_t *page)
{
    uint32_t crc = UINT32_MAX;
    for (size_t offset = 0; offset < EXODUS_PAGE_BYTES; ++offset) {
        crc ^= page[offset];
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static bool metadata_valid(const struct exodus_page_integrity_store *store)
{
    if (!store || !store->pages.payload || !store->pages.page_ends ||
        !store->checksums || store->pages.page_count > store->pages.page_capacity ||
        store->pages.page_count > store->checksum_capacity ||
        store->pages.payload_used > store->pages.payload_capacity)
        return false;

    size_t previous = 0;
    for (size_t index = 0; index < store->pages.page_count; ++index) {
        size_t end = store->pages.page_ends[index];
        if (end <= previous || end > store->pages.payload_used)
            return false;
        previous = end;
    }
    return previous == store->pages.payload_used;
}

static bool append_page(struct exodus_page_integrity_store *store,
                        const uint8_t *page, bool adaptive)
{
    if (!page || !metadata_valid(store) ||
        store->pages.page_count == store->pages.page_capacity ||
        store->pages.page_count == store->checksum_capacity)
        return false;

    size_t index = store->pages.page_count;
    uint32_t checksum = page_crc32(page);
    bool appended = adaptive ? exodus_page_store_append_adaptive(&store->pages, page)
                             : exodus_page_store_append(&store->pages, page);
    if (!appended)
        return false;

    store->checksums[index] = checksum;
    return true;
}

bool exodus_page_integrity_store_append(struct exodus_page_integrity_store *store,
                                        const uint8_t *page)
{
    return append_page(store, page, false);
}

bool exodus_page_integrity_store_append_adaptive(
    struct exodus_page_integrity_store *store, const uint8_t *page)
{
    return append_page(store, page, true);
}

bool exodus_page_integrity_store_restore(
    const struct exodus_page_integrity_store *store, size_t index, uint8_t *page)
{
    if (!page || !metadata_valid(store) || index >= store->pages.page_count)
        return false;

    uint8_t scratch[EXODUS_PAGE_BYTES];
    if (!exodus_page_store_restore(&store->pages, index, scratch) ||
        page_crc32(scratch) != store->checksums[index])
        return false;

    memcpy(page, scratch, sizeof(scratch));
    return true;
}
