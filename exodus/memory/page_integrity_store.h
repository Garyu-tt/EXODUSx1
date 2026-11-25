#ifndef EXODUS_PAGE_INTEGRITY_STORE_H
#define EXODUS_PAGE_INTEGRITY_STORE_H

#include "page_store.h"

/* Optional integrity sidecar for an ordinary packed page store.
 *
 * Initialize pages exactly as for exodus_page_store, provide a separate
 * caller-owned array of at least checksum_capacity uint32_t entries, and
 * leave every counter at zero. The payload, end offsets and checksums are one
 * immutable group after construction. Serialize all access.
 */
struct exodus_page_integrity_store {
    struct exodus_page_store pages;
    uint32_t *checksums;
    size_t checksum_capacity;
};

/* Append one page and its IEEE CRC-32. Rejection changes neither the packed
 * store nor the checksum array. The legacy function keeps sparse/raw encoding. */
bool exodus_page_integrity_store_append(struct exodus_page_integrity_store *store,
                                        const uint8_t *page);

/* As above, selecting the smallest encoding supported by the adaptive codec. */
bool exodus_page_integrity_store_append_adaptive(
    struct exodus_page_integrity_store *store, const uint8_t *page);

/* Restore through a bounded scratch page. Invalid metadata, malformed payload
 * or a checksum mismatch leaves the caller's destination page untouched. */
bool exodus_page_integrity_store_restore(
    const struct exodus_page_integrity_store *store, size_t index, uint8_t *page);

#endif
