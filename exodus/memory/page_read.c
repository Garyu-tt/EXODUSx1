#include "page_read.h"
#include <string.h>

enum { WORD_BYTES = 4, WORDS = EXODUS_PAGE_BYTES / WORD_BYTES,
       BLOCK_BYTES = 64, BLOCKS = EXODUS_PAGE_BYTES / BLOCK_BYTES,
       MASK_BYTES = BLOCKS / 8 };

/* Bytewise population count, independent of host endianness and alignment.
 * The sparse bitmap stores eight blocks in each byte. */
static unsigned mask_popcount(unsigned x)
{
    x-= (x>>1)&0x55u;
    x=(x&0x33u)+((x>>2)&0x33u);
    return (x+(x>>4))&0x0fu;
}

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
        size_t target=byte/BLOCK_BYTES, group=target/8;
        unsigned bit=1u<<(target%8);
        size_t total=0,rank=0;
        /* Count the full mask before reading any payload, even when the
         * requested block is absent or corruption occurs after it. */
        for(size_t i=0;i<MASK_BYTES;i++){
            unsigned count=mask_popcount(encoded[i]);
            total+=count;
            if(i<group)rank+=count;
        }
        if(MASK_BYTES+total*BLOCK_BYTES!=size)return false;
        if(encoded[group]&bit){
            rank+=mask_popcount(encoded[group]&(bit-1u));
            size_t location=MASK_BYTES+rank*BLOCK_BYTES+byte%BLOCK_BYTES;
            memcpy(result,encoded+location,WORD_BYTES);
        }
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
    size_t physical=s->page_refs[index];
    if(physical>=s->unique.page_count)return false;
    const struct exodus_hot_entry *h=exodus_hot_find(s,physical);
    if(h)return exodus_bdi_slab_read(s->slab,&h->page,word,out);
    return exodus_page_store_read_word(&s->unique, physical, word, out);
}
