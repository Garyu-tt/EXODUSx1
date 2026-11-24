#ifndef EXODUS_PAGE_DEDUP_STORE_H
#define EXODUS_PAGE_DEDUP_STORE_H

#include "page_store.h"

/* Immutable group of logical pages backed by one copy of each unique page.
 * Supply separate caller-owned buffers: unique_pages.payload and page_ends,
 * fingerprints[unique_pages.page_capacity], and page_refs[page_capacity].
 * Initialize every counter to zero. While the group is live, only these
 * functions may modify its contents; do not mutate the embedded page store.
 * Serialize access and retain all buffers for the lifetime of the group. */
struct exodus_page_dedup_store {
    struct exodus_page_store unique_pages;
    uint64_t *fingerprints;
    size_t *page_refs;
    size_t page_capacity;
    size_t page_count;
};

/* Append one readable EXODUS_PAGE_BYTES page at the next logical index.
 * A fingerprint match is verified against all decoded bytes before sharing.
 * An identical page needs only a free logical index, even if unique storage
 * is full. Failure leaves all payload, metadata and counters unchanged. */
bool exodus_page_dedup_store_append(struct exodus_page_dedup_store *store, const uint8_t *page);

/* Restore a logical page into a separate writable EXODUS_PAGE_BYTES buffer.
 * Invalid indices, references and malformed compressed payloads leave the
 * destination unchanged. The caller may freely modify the restored copy. */
bool exodus_page_dedup_store_restore(const struct exodus_page_dedup_store *store,
                                   size_t index, uint8_t *page);

#endif
