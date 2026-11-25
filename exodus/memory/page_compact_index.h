#ifndef EXODUS_PAGE_COMPACT_INDEX_H
#define EXODUS_PAGE_COMPACT_INDEX_H

#include "page_store.h"

enum { EXODUS_COMPACT_INDEX_STRIDE = 32 };

/* A read-only view of the existing payload and the compact caller-owned index. */
struct exodus_page_compact_index {
    const uint8_t *payload;
    const uint16_t *lengths;
    const size_t *checkpoints;
    size_t payload_used;
    size_t page_count;
};

/* One checkpoint per group of up to EXODUS_COMPACT_INDEX_STRIDE pages. */
size_t exodus_page_compact_checkpoints(size_t page_count);

/* Supply one length per page and exodus_page_compact_checkpoints(page_count)
 * checkpoints. The output buffers and index must not overlap each other or
 * the source store or its buffers. An empty store needs neither array.
 *
 * Failure leaves the index and both output arrays untouched. After success
 * the source store and its end offsets can be released. Keep the payload,
 * lengths and checkpoints alive and immutable until the last restore. */
bool exodus_page_compact_build(const struct exodus_page_store *store,
                               struct exodus_page_compact_index *index,
                               uint16_t *lengths, size_t length_capacity,
                               size_t *checkpoints, size_t checkpoint_capacity);

/* Restore a single page through the existing decoder into a separate
 * EXODUS_PAGE_BYTES buffer. At most 31 earlier lengths are consulted. */
bool exodus_page_compact_restore(const struct exodus_page_compact_index *index,
                                 size_t page_index, uint8_t *page);

#endif
