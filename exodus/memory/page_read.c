#include "page_read.h"
#include <string.h>

enum { WORD_BYTES = 4, WORDS = EXODUS_PAGE_BYTES / WORD_BYTES,
       BLOCK_BYTES = 64, BLOCKS = EXODUS_PAGE_BYTES / BLOCK_BYTES,
       MASK_BYTES = BLOCKS / 8 };

bool exodus_page_read_word(const uint8_t *encoded, size_t size, size_t word,
                           uint8_t out[4])
{
    if (!encoded || !out || word >= WORDS || !size || size > EXODUS_PAGE_BYTES)
        return false;
    uint8_t result[WORD_BYTES] = {0};
    size_t byte = word * WORD_BYTES;
    if (size == EXODUS_PAGE_BYTES) {
        memcpy(result, encoded + byte, WORD_BYTES);
    } else if (size & 1u) {
        if (encoded[0] == EXODUS_PAGE_BDI_VERSION)
            return exodus_page_bdi_read_word(encoded, size, word, out);
        if (encoded[0] == EXODUS_PAGE_REPEATED_TILE_VERSION) {
            size_t tile = size - 1;
            if (tile != 8 && tile != 16) return false;
            memcpy(result, encoded + 1 + byte % tile, WORD_BYTES);
        } else if (encoded[0] == 1) {
            size_t off = 1, first = 0;
            while (first < WORDS) {
                if (off == size) return false;
                unsigned command = encoded[off++];
                size_t count = (command & 127u) + 1;
                bool repeat = (command & 128u) != 0;
                size_t bytes = repeat ? WORD_BYTES : count * WORD_BYTES;
                if ((repeat && count < 2) || count > WORDS - first || bytes > size - off)
                    return false;
                if (word >= first && word - first < count)
                    memcpy(result, encoded + off + (repeat ? 0 : (word - first) * WORD_BYTES), WORD_BYTES);
                off += bytes;
                first += count;
            }
            if (off != size && !(off % 2 == 0 && off + 1 == size && encoded[off] == 0))
                return false;
        } else return false;
    } else {
        if (size < MASK_BYTES || (size - MASK_BYTES) % BLOCK_BYTES != 0)
            return false;
        size_t expected = MASK_BYTES, target = byte / BLOCK_BYTES, location = 0;
        bool present = false;
        for (size_t block = 0; block < BLOCKS; ++block) {
            if (!(encoded[block / 8] & (1u << (block % 8)))) continue;
            if (block == target) { location = expected + byte % BLOCK_BYTES; present = true; }
            expected += BLOCK_BYTES;
        }
        if (expected != size) return false;
        if (present) memcpy(result, encoded + location, WORD_BYTES);
    }
    memcpy(out, result, WORD_BYTES);
    return true;
}

bool exodus_page_store_read_word(const struct exodus_page_store *s,
                                 size_t index, size_t word, uint8_t out[4])
{
    if (!s || !s->payload || !exodus_page_store_has_index(s) || index >= s->page_count ||
        s->page_count > s->page_capacity || s->payload_used > s->payload_capacity)
        return false;
    size_t start,end;
    if(!exodus_page_store_extent(s,index,&start,&end))return false;
    return exodus_page_read_word(s->payload + start, end - start, word, out);
}

bool exodus_page_dedup_store_read_word(const struct exodus_page_dedup_store *s,
                                       size_t index, size_t word, uint8_t out[4])
{
    if (!s || !s->fingerprints || !s->page_refs || !s->unique.payload || !exodus_page_store_has_index(&s->unique) ||
        index >= s->page_count || s->page_count > s->page_capacity ||
        !s->unique.page_count || s->unique.page_count > s->unique.page_capacity ||
        s->unique.page_count > s->page_count || !s->unique.payload_used ||
        s->unique.payload_used > s->unique.payload_capacity ||
        (!exodus_page_store_is_pool(&s->unique) && s->unique.page_ends[s->unique.page_count - 1] != s->unique.payload_used))
        return false;
    return exodus_page_store_read_word(&s->unique, s->page_refs[index], word, out);
}
