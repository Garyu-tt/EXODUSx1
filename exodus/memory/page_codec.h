#ifndef EXODUS_PAGE_CODEC_H
#define EXODUS_PAGE_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { EXODUS_PAGE_BYTES = 4096 };
enum { EXODUS_PAGE_REPEATED_TILE_VERSION = 2, EXODUS_PAGE_BDI_VERSION = 3 };

/* Supply two separate buffers of EXODUS_PAGE_BYTES bytes. Retain only the
 * returned number of encoded bytes and record that length with the page. */
size_t exodus_page_encode(const uint8_t *page, uint8_t *encoded);

/* As above, also trying full-page 8-/16-byte tile repeats 32-bit word runs and BDI blocks.
 * Decode adaptive output with this version of exodus_page_decode. */
size_t exodus_page_encode_adaptive(const uint8_t *page, uint8_t *encoded);

/* Supply encoded_size readable bytes and a separate writable page buffer.
 * Invalid encodings leave the page untouched. */
bool exodus_page_decode(const uint8_t *encoded, size_t encoded_size, uint8_t *page);

/* Compare any existing encoding with a logical page without full decoding.
 * Returns false for malformed input and leaves equal unchanged; otherwise
 * equal reports exact logical equality, not merely a fingerprint match. */
bool exodus_page_compare(const uint8_t *encoded,size_t size,const uint8_t *page,bool *equal);

/* Read a word from a validated V3 BDI page without full decoding. */
bool exodus_page_bdi_read_word(const uint8_t *encoded, size_t size, size_t word, uint8_t out[4]);

/* Shared block encoding: 64 readable bytes, separate 64-byte output.
 * NULL output only classifies the block; no bytes are written.
 * Returns 20 (BDI) or 64 (RAW). Decode/read reject other lengths. */
size_t exodus_bdi_block_encode(const uint8_t *block,uint8_t *out);
bool exodus_bdi_block_decode(const uint8_t *encoded,size_t n,uint8_t *out);
bool exodus_bdi_block_read(const uint8_t *encoded,size_t n,size_t word,uint8_t out[4]);

/* Internal shared fast-path predicate for a 20-byte packed block.
 * True only if changing one delta preserves canonical encoding.
 * On false, delta is untouched; callers use the general encoder.
 * Inline to avoid adding a call to the hot slab write path. */
static inline bool exodus_bdi_direct_delta(const uint8_t *src,size_t word,
                                           const uint8_t value[4],uint8_t *delta)
{
    if(!src||!value||!delta||word>=16)return false;
    uint32_t base=(uint32_t)src[0]|(uint32_t)src[1]<<8|
                  (uint32_t)src[2]<<16|(uint32_t)src[3]<<24;
    uint32_t next=(uint32_t)value[0]|(uint32_t)value[1]<<8|
                  (uint32_t)value[2]<<16|(uint32_t)value[3]<<24;
    if(next<base||next-base>255u)return false;
    uint8_t d=(uint8_t)(next-base);
    bool minimum=d==0;
    for(size_t i=0;i<16;i++){
        if(i==word)continue;
        if((uint32_t)src[4+i]>UINT32_MAX-base)return false;
        if(src[4+i]==0)minimum=true;
    }
    if(!minimum)return false;
    *delta=d;
    return true;
}

/* Prepare a single replacement block in independent scratch storage. The
 * original encoded buffer is unchanged; output unchanged on invalid input. */
struct exodus_bdi_patch {
    size_t offset, old_len, new_len;
    uint8_t bytes[64];
};
bool exodus_page_bdi_prepare_patch(const uint8_t *encoded,size_t size,size_t word,
                                   const uint8_t value[4],struct exodus_bdi_patch *out);

/* Patch only the target 64-byte block; other encoded blocks are copied.
 * Output must be a separate 4096-byte buffer. Zero means invalid input or
 * the resulting BDI representation would not fit; output stays untouched. */
size_t exodus_page_bdi_patch_word(const uint8_t *encoded,size_t size,size_t word,
                                  const uint8_t value[4],uint8_t *out);
/* Logical position-dependent fingerprint v2, identical to the raw page hash.
 * Hashes validated logical words directly or stages one 64-byte block. */
bool exodus_page_bdi_fingerprint(const uint8_t *encoded,size_t size,uint64_t *out);
bool exodus_page_bdi_equal_page(const uint8_t *encoded,size_t size,const uint8_t *page);

/* Non-cryptographic candidate filter. Hash metadata is runtime-only;
 * old FNV-1a indices must be rebuilt before use with this version.
 * page points to 4096 readable bytes. */
enum { EXODUS_PAGE_FINGERPRINT_VERSION = 2 };
uint64_t exodus_page_fingerprint(const uint8_t *page);
/* O(1) update of a trusted v2 hash. The word is a zero-based 4-byte index.
 * A full byte comparison remains mandatory before deduplicating. */
bool exodus_page_fingerprint_update(uint64_t old,size_t word,
                                     const uint8_t before[4],const uint8_t after[4],
                                     uint64_t *out);

#endif
