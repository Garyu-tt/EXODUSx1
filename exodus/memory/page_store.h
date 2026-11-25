#ifndef EXODUS_PAGE_STORE_H
#define EXODUS_PAGE_STORE_H

#include "page_codec.h"

/* Initialize with separate caller-owned payload and page_ends buffers and
 * their capacities. Leave the counters at zero. Serialize access and let
 * the store functions update the contents and counters. */
struct exodus_page_store {
    uint8_t *payload;
    size_t payload_capacity;
    size_t *page_ends;
    size_t page_capacity;
    size_t payload_used;
    size_t page_count;
};

/* Append one readable EXODUS_PAGE_BYTES page using the legacy sparse/raw
 * encoder. A capacity failure leaves the store unchanged. */
bool exodus_page_store_append(struct exodus_page_store *store, const uint8_t *page);

/* Append with the adaptive encoder. Capacity is
 * checked against the selected encoded length. May share a store with legacy
 * appends; a capacity failure leaves the store unchanged. */
bool exodus_page_store_append_adaptive(struct exodus_page_store *store, const uint8_t *page);

/* Replace an existing page without changing its index or the page count.
 * Shifts the following payloads and adjusts their offsets when the encoded
 * size changes. Invalid indices, inconsistent offsets or insufficient capacity
 * leave the store unchanged. Access must be serialized by the caller. */
bool exodus_page_store_replace(struct exodus_page_store *store, size_t index, const uint8_t *page);

/* Replace using the adaptive encoder, retaining the same failure atomicity
 * and logical indices. Not for immutable shared backing or integrity wrappers
 * whose checksums must be updated by their own API. */
bool exodus_page_store_replace_adaptive(struct exodus_page_store *store, size_t index,
                                        const uint8_t *page);

/* Restore into a separate EXODUS_PAGE_BYTES buffer. Invalid indices or
 * malformed compressed encodings leave the destination unchanged. */
bool exodus_page_store_restore(const struct exodus_page_store *store, size_t index, uint8_t *page);

#endif
