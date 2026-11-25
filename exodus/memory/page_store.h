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
    /* Optional caller-owned array of page_capacity starts, set on empty store.
     * NULL selects packed layout; non-NULL selects reusable extents. In pool
     * mode page_ends are absolute ends and payload_used is live bytes. */
    size_t *page_starts;
    /* Optional uint32_t[page_capacity], initialized on an empty pool.
     * Stores physical record indices sorted by start; all arrays disjoint.
     * Requires page_capacity <= UINT32_MAX. NULL retains legacy pool. */
    uint32_t *page_order;
    /* Compact pool: use starts32 + lengths16 instead of page_starts/page_ends.
     * Requires page_order and capacity <= UINT32_MAX bytes. Arrays separate.
     * Initialize on an empty store. Lengths are 1..4096. */
    uint32_t *starts32;
    uint16_t *lengths16;
};

/* Append one readable EXODUS_PAGE_BYTES page using the legacy sparse/raw
 * encoder. A capacity failure leaves the store unchanged. */
bool exodus_page_store_append(struct exodus_page_store *store, const uint8_t *page);

/* Append with the adaptive encoder. Capacity is
 * checked against the selected encoded length. May share a store with legacy
 * appends; a capacity failure leaves the store unchanged. */
bool exodus_page_store_append_adaptive(struct exodus_page_store *store, const uint8_t *page);

/* Replace an existing page without changing its index or the page count.
 * Packed mode shifts the following payloads when the size changes; pool
 * mode reuses a free extent or fails without moving unrelated payloads. Invalid indices, inconsistent offsets or insufficient capacity
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

/* Layout predicates do not validate metadata. */
static inline bool exodus_page_store_is_pool(const struct exodus_page_store *s)
{ return s && (s->page_starts || s->starts32); }
static inline bool exodus_page_store_has_index(const struct exodus_page_store *s)
{ return s && (s->page_ends || (s->starts32 && s->lengths16)); }

/* Validate all active extents, including overlap and live byte accounting.
 * Indexed pool mode is linear; unindexed pool mode is quadratic. */
bool exodus_page_store_validate(const struct exodus_page_store *store);
/* Locate one record (local bounds validation). */
bool exodus_page_store_extent(const struct exodus_page_store *store,size_t index,
                              size_t *start,size_t *end);
/* Internal pool operations for an already encoded, separate source buffer.
 * Do not use directly on a dedup/integrity store outside its own wrapper. */
/* Preflight under the same serialized access as the ensuing put. */
bool exodus_page_store_pool_can_put(const struct exodus_page_store *,size_t,size_t);
bool exodus_page_store_pool_put(struct exodus_page_store *store,size_t index,
                                const uint8_t *encoded,size_t size);
bool exodus_page_store_pool_remove(struct exodus_page_store *store,size_t index);
struct exodus_page_pool_stats {
    size_t free_bytes, largest_gap, tail_free, holes;
};
/* Indexed pools only. Output unchanged on invalid metadata. */
bool exodus_page_store_pool_stats(const struct exodus_page_store *store,
                                  struct exodus_page_pool_stats *out);
/* Compact toward offset zero, preserving all page indices. Stop before a
 * record would exceed the copy budget; later calls can resume. Zero budget
 * is valid. Returns copied bytes, unchanged on invalid metadata.
 * Requires an indexed pool and serialized access. */
bool exodus_page_store_pool_compact(struct exodus_page_store *store,
                                    size_t budget,size_t *moved);
/* A platform callback discards physical backing for a free aligned range.
 * It MUST keep the virtual mapping writable and usable without explicit
 * recommit (e.g. Linux MADV_DONTNEED on private anonymous memory).
 * It must not reenter the store or modify metadata or live bytes. */
typedef bool (*exodus_page_discard_fn)(void *context,void *address,size_t bytes);
/* Indexed pool only. Base must be granule-aligned. Discard only full granules
 * entirely free, up to budget bytes. No logical metadata changes. On invalid
 * input, output unchanged. On callback failure, return false and report bytes
 * accepted by prior callbacks. Free bytes have unspecified contents. Counts
 * successful discard requests, not measured RSS; repeated calls may revisit
 * ranges. Serialize access and use a dedicated compatible mapping. */
bool exodus_page_store_pool_discard_free(struct exodus_page_store *store,
                                        size_t granule,size_t budget,
                                        exodus_page_discard_fn discard,void *context,
                                        size_t *discarded);
#endif
