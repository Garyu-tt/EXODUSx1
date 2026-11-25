#ifndef EXODUS_PAGE_DEDUP_STORE_H
#define EXODUS_PAGE_DEDUP_STORE_H

#include "page_store.h"
#include "bdi_slab.h"

struct exodus_hot_entry {
    size_t physical;
    struct exodus_bdi_slab_page page;
};

/* A caller-owned group of logical pages sharing immutable backing.
 * Initialize both page counts and unique.payload_used to zero. Supply separate,
 * non-overlapping buffers for unique.payload, unique.page_ends, fingerprints,
 * and page_refs (and optional unique.page_starts and unique.page_order, or compact starts32/lengths16 and page_order). fingerprints has unique.page_capacity uint64_t entries;
 * these are runtime v2 indices, not serialized FNV-1a values. Rebuild old
 * groups from decoded pages; do not reuse old fingerprint arrays.
 * page_refs has page_capacity size_t entries. The unique store belongs to this
 * wrapper exclusively: do not append, replace, or modify its pages directly.
 * Serialize all access and keep these buffers alive for the group's lifetime.
 */
struct exodus_page_dedup_store {
    struct exodus_page_store unique;
    uint64_t *fingerprints;
    size_t *page_refs;
    size_t page_capacity;
    size_t page_count;
    /* Optional bounded hot table; entries initially zero, indexed pool only. */
    struct exodus_bdi_slab *slab;
    struct exodus_hot_entry *hot;
    size_t hot_capacity;
};

/* Append one readable EXODUS_PAGE_BYTES source page, assigning the next logical
 * index. The source must not overlap any store buffer. After success the caller
 * may reuse the source. Equal pages share backing only after a full byte check;
 * a fingerprint match alone never establishes equality. A duplicate can fit
 * even when the unique-page or payload capacity is exhausted. Failure leaves
 * all buffers and counters unchanged. Append validates every physical end
 * offset before admitting either a duplicate or a new unique page.
 */
bool exodus_page_dedup_store_append(struct exodus_page_dedup_store *store,
                                   const uint8_t *page);

/* Same immutable sharing contract, with adaptive encoding of new unique pages.
 * Existing logical and physical pages are never re-encoded. Both append
 * variants may share a group; equality is always checked on decoded bytes. */
bool exodus_page_dedup_store_append_adaptive(struct exodus_page_dedup_store *store,
                                            const uint8_t *page);

/* Restore a logical page into a separate writable EXODUS_PAGE_BYTES buffer.
 * The output must not overlap any store buffer. Invalid indices, mappings, or
 * malformed stored encodings leave the output unchanged. Changes to a restored page do not alter stored pages.
 */
bool exodus_page_dedup_store_restore(const struct exodus_page_dedup_store *store,
                                    size_t index, uint8_t *page);

/* Replace a logical page, preserving other owners and deduplicating by full
 * decoded equality. Reclaims the old backing when its last owner leaves.
 * Source must not overlap any store buffer. Capacity or validation failure
 * leaves buffers and counters unchanged. Serialize access. */
bool exodus_page_dedup_store_replace_adaptive(struct exodus_page_dedup_store *store,
                                             size_t index, const uint8_t *page);
/* Copy-on-write of a 4-byte word in stored byte order. BDI patches only the target block and updates its trusted v2 hash in O(1). Other
 * formats and BDI overflow fall back to full page re-encoding. BDI may
 * retain a valid, but not globally minimal, representation until re-encoded. */
bool exodus_page_dedup_store_write_word(struct exodus_page_dedup_store *store,
                                       size_t index, size_t word,
                                       const uint8_t value[4]);
/* Compact physical extents without changing logical references or hashes.
 * Same copy budget contract as exodus_page_store_pool_compact. */
bool exodus_page_dedup_store_compact(struct exodus_page_dedup_store *store,
                                     size_t budget,size_t *moved);
/* Release only free granules in the shared physical pool. Same mapping,
 * budget and partial-failure contract as page_store_pool_discard_free. */
bool exodus_page_dedup_store_discard_free(struct exodus_page_dedup_store *store,
                                         size_t granule,size_t budget,
                                         exodus_page_discard_fn discard,void *context,
                                         size_t *discarded);
/* Migrate the physical page shared by this logical index. All aliases retain
 * their identity. Hot records reserve an eight-byte cold placeholder.
 * On failure logical contents and mappings remain unchanged. */
bool exodus_page_dedup_store_promote(struct exodus_page_dedup_store *,size_t);
bool exodus_page_dedup_store_demote(struct exodus_page_dedup_store *,size_t);
static inline const struct exodus_hot_entry *exodus_hot_find(const struct exodus_page_dedup_store *s,size_t physical)
{
    if(!s->slab||!s->hot)return NULL;
    for(size_t i=0;i<s->hot_capacity;i++)
        if(s->hot[i].page.active&&s->hot[i].physical==physical)return &s->hot[i];
    return NULL;
}
#endif
