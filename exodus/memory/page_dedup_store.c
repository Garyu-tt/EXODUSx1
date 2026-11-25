#include "page_dedup_store.h"

#include <string.h>

static bool valid_store(const struct exodus_page_dedup_store *store)
{
    return store && store->fingerprints && store->page_refs &&
        store->unique.payload && store->unique.page_ends &&
        store->page_count <= store->page_capacity &&
        store->unique.page_count <= store->unique.page_capacity &&
        store->unique.page_count <= store->page_count &&
        ((store->unique.page_count == 0) == (store->page_count == 0)) &&
        store->unique.payload_used <= store->unique.payload_capacity &&
        ((store->unique.page_count == 0) == (store->unique.payload_used == 0)) &&
        (store->unique.page_count == 0 ||
         store->unique.page_ends[store->unique.page_count - 1] == store->unique.payload_used);
}

static uint64_t page_fingerprint(const uint8_t *page)
{
    /* FNV-1a only filters candidates. Restored bytes decide equality. */
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i) {
        hash ^= page[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool append_page(struct exodus_page_dedup_store *store,
                        const uint8_t *page, bool adaptive)
{
    if (!page || !valid_store(store) || store->page_count == store->page_capacity)
        return false;

    /* Appending must not extend an inconsistent physical index, even
     * when no fingerprint matches or an early match would skip its tail.
     * Restore keeps its indexed, single-record validation path. */
    size_t previous = 0;
    for (size_t i = 0; i < store->unique.page_count; ++i) {
        size_t end = store->unique.page_ends[i];
        if (end <= previous || end > store->unique.payload_used ||
            end - previous > EXODUS_PAGE_BYTES)
            return false;
        previous = end;
    }

    uint64_t hash = page_fingerprint(page);
    size_t physical = store->unique.page_count;
    for (size_t i = 0; i < store->unique.page_count; ++i) {
        if (store->fingerprints[i] != hash)
            continue;
        uint8_t restored[EXODUS_PAGE_BYTES];
        if (!exodus_page_store_restore(&store->unique, i, restored))
            return false;
        if (memcmp(restored, page, sizeof(restored)) == 0) {
            physical = i;
            break;
        }
    }

    if (physical == store->unique.page_count) {
        if (!(adaptive ? exodus_page_store_append_adaptive(&store->unique, page)
                       : exodus_page_store_append(&store->unique, page)))
            return false;
        store->fingerprints[physical] = hash;
    }
    store->page_refs[store->page_count] = physical;
    ++store->page_count;
    return true;
}

bool exodus_page_dedup_store_append(struct exodus_page_dedup_store *store,
                                   const uint8_t *page)
{
    return append_page(store, page, false);
}

bool exodus_page_dedup_store_append_adaptive(struct exodus_page_dedup_store *store,
                                            const uint8_t *page)
{
    return append_page(store, page, true);
}

bool exodus_page_dedup_store_restore(const struct exodus_page_dedup_store *store,
                                    size_t index, uint8_t *page)
{
    if (!page || !valid_store(store) || index >= store->page_count)
        return false;
    return exodus_page_store_restore(&store->unique, store->page_refs[index], page);
}

/* Copy-on-write for the immutable physical backing. Logical indices never move.
 * No allocations: the existing physical table is compacted only when a sole
 * owner switches to an already stored page. Caller serializes access. */
bool exodus_page_dedup_store_replace_adaptive(struct exodus_page_dedup_store *s,
                                             size_t index, const uint8_t *page)
{
    if (!page || !valid_store(s) || index >= s->page_count)
        return false;
    size_t previous = 0;
    for (size_t i = 0; i < s->unique.page_count; ++i) {
        size_t end = s->unique.page_ends[i];
        if (end <= previous || end > s->unique.payload_used ||
            end - previous > EXODUS_PAGE_BYTES) return false;
        previous = end;
    }
    size_t old = s->page_refs[index], owners = 0;
    if (old >= s->unique.page_count) return false;
    for (size_t i = 0; i < s->page_count; ++i) {
        if (s->page_refs[i] >= s->unique.page_count) return false;
        if (s->page_refs[i] == old) ++owners;
    }
    uint8_t restored[EXODUS_PAGE_BYTES];
    if (!exodus_page_store_restore(&s->unique, old, restored)) return false;
    if (memcmp(restored, page, sizeof(restored)) == 0) return true;
    uint64_t hash = page_fingerprint(page);
    size_t match = s->unique.page_count;
    for (size_t i = 0; i < s->unique.page_count; ++i) {
        if (i == old || s->fingerprints[i] != hash) continue;
        if (!exodus_page_store_restore(&s->unique, i, restored)) return false;
        if (memcmp(restored, page, sizeof(restored)) == 0) {
            match = i;
            break;
        }
    }
    if (match == s->unique.page_count) {
        if (owners == 1) {
            if (!exodus_page_store_replace_adaptive(&s->unique, old, page))
                return false;
            s->fingerprints[old] = hash;
        } else {
            if (s->unique.page_count >= s->page_count ||
                !exodus_page_store_append_adaptive(&s->unique, page))
                return false;
            s->fingerprints[match] = hash;
            s->page_refs[index] = match;
        }
        return true;
    }
    s->page_refs[index] = match;
    if (owners == 1) {
        /* All validation is complete; no failure points after mutation. */
        size_t start = old ? s->unique.page_ends[old - 1] : 0;
        size_t end = s->unique.page_ends[old], bytes = end - start;
        memmove(s->unique.payload + start, s->unique.payload + end,
                s->unique.payload_used - end);
        for (size_t i = old + 1; i < s->unique.page_count; ++i) {
            s->unique.page_ends[i - 1] = s->unique.page_ends[i] - bytes;
            s->fingerprints[i - 1] = s->fingerprints[i];
        }
        for (size_t i = 0; i < s->page_count; ++i)
            if (s->page_refs[i] > old) --s->page_refs[i];
        --s->unique.page_count;
        s->unique.payload_used -= bytes;
    }
    return true;
}

bool exodus_page_dedup_store_write_word(struct exodus_page_dedup_store *s,
                                       size_t index, size_t word,
                                       const uint8_t value[4])
{
    uint8_t page[EXODUS_PAGE_BYTES];
    if (!value || word >= EXODUS_PAGE_BYTES / 4 ||
        !exodus_page_dedup_store_restore(s, index, page)) return false;
    memcpy(page + word * 4, value, 4);
    return exodus_page_dedup_store_replace_adaptive(s, index, page);
}
