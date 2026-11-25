#include "page_compact_index.h"

size_t exodus_page_compact_checkpoints(size_t page_count)
{
    return page_count / EXODUS_COMPACT_INDEX_STRIDE +
           (page_count % EXODUS_COMPACT_INDEX_STRIDE != 0);
}

bool exodus_page_compact_build(const struct exodus_page_store *store,
                               struct exodus_page_compact_index *index,
                               uint16_t *lengths, size_t length_capacity,
                               size_t *checkpoints, size_t checkpoint_capacity)
{
    if (!store || !index || store->page_starts || store->page_count > store->page_capacity ||
        store->payload_used > store->payload_capacity ||
        store->page_count > length_capacity ||
        exodus_page_compact_checkpoints(store->page_count) > checkpoint_capacity)
        return false;
    if (store->page_count && (!store->page_ends || !store->payload || !lengths || !checkpoints))
        return false;

    size_t start = 0;
    for (size_t i = 0; i < store->page_count; ++i) {
        size_t end = store->page_ends[i];
        if (end <= start || end > store->payload_used || end - start > EXODUS_PAGE_BYTES)
            return false;
        start = end;
    }
    if (start != store->payload_used)
        return false;

    start = 0;
    for (size_t i = 0; i < store->page_count; ++i) {
        if (i % EXODUS_COMPACT_INDEX_STRIDE == 0)
            checkpoints[i / EXODUS_COMPACT_INDEX_STRIDE] = start;
        lengths[i] = (uint16_t)(store->page_ends[i] - start);
        start = store->page_ends[i];
    }
    *index = (struct exodus_page_compact_index){
        .payload = store->payload,
        .lengths = lengths,
        .checkpoints = checkpoints,
        .payload_used = store->payload_used,
        .page_count = store->page_count
    };
    return true;
}

bool exodus_page_compact_restore(const struct exodus_page_compact_index *index,
                                 size_t page_index, uint8_t *page)
{
    if (!index || !page || page_index >= index->page_count ||
        !index->payload || !index->lengths || !index->checkpoints)
        return false;

    size_t group = page_index / EXODUS_COMPACT_INDEX_STRIDE;
    size_t start = index->checkpoints[group];
    size_t limit = index->payload_used;
    if (group + 1 < exodus_page_compact_checkpoints(index->page_count))
        limit = index->checkpoints[group + 1];
    if (start >= limit || limit > index->payload_used)
        return false;

    for (size_t i = group * EXODUS_COMPACT_INDEX_STRIDE; i <= page_index; ++i) {
        size_t length = index->lengths[i];
        if (!length || length > EXODUS_PAGE_BYTES || length > limit - start)
            return false;
        if (i == page_index)
            return exodus_page_decode(index->payload + start, length, page);
        start += length;
    }
    return false;
}
