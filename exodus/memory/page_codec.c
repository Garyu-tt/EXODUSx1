#include "page_codec.h"

#include <string.h>

enum {
    BLOCK_BYTES = 64,
    BLOCKS = EXODUS_PAGE_BYTES / BLOCK_BYTES,
    MASK_BYTES = BLOCKS / 8,
    WORD_BYTES = 4,
    PAGE_WORDS = EXODUS_PAGE_BYTES / WORD_BYTES,
    WORD_RUN_VERSION = 1,
    WORD_RUN_REPEAT = 128,
    WORD_RUN_MAX = 128
};

static bool next_word_matches(const uint8_t *page, size_t word)
{
    return word + 1 < PAGE_WORDS &&
           memcmp(page + word * WORD_BYTES, page + (word + 1) * WORD_BYTES, WORD_BYTES) == 0;
}

static size_t encode_word_runs(const uint8_t *page, uint8_t *encoded, size_t fallback_size)
{
    size_t offset = 1;
    encoded[0] = WORD_RUN_VERSION;

    for (size_t word = 0; word < PAGE_WORDS;) {
        bool repeated = next_word_matches(page, word);
        size_t count = 1;
        if (repeated) {
            count = 2;
            while (count < WORD_RUN_MAX && next_word_matches(page, word + count - 1))
                ++count;
        } else {
            while (count < WORD_RUN_MAX && word + count < PAGE_WORDS &&
                   !next_word_matches(page, word + count))
                ++count;
        }

        size_t payload_size = repeated ? WORD_BYTES : count * WORD_BYTES;
        if (1 + payload_size >= fallback_size - offset)
            return 0;
        uint8_t control = (uint8_t)(count - 1);
        if (repeated)
            control |= WORD_RUN_REPEAT;
        encoded[offset++] = control;
        memcpy(encoded + offset, page + word * WORD_BYTES, payload_size);
        offset += payload_size;
        word += count;
    }

    /* Sparse and raw lengths are even. Padding can never reach the even
     * fallback size because the unpadded size is already smaller. */
    if (offset % 2 == 0)
        encoded[offset++] = 0;
    return offset;
}

static size_t encode_repeated_tile(const uint8_t *page, uint8_t *encoded)
{
    size_t tile_size;
    /* Keep single-word fills on the existing word-run format. */
    if (memcmp(page, page + WORD_BYTES, WORD_BYTES) != 0 &&
        memcmp(page, page + 8, EXODUS_PAGE_BYTES - 8) == 0)
        tile_size = 8;
    else if (memcmp(page, page + 8, 8) != 0 &&
             memcmp(page, page + 16, EXODUS_PAGE_BYTES - 16) == 0)
        tile_size = 16;
    else
        return 0;

    encoded[0] = EXODUS_PAGE_REPEATED_TILE_VERSION;
    memcpy(encoded + 1, page, tile_size);
    return 1 + tile_size;
}

static size_t encode_page(const uint8_t *page, uint8_t *encoded, bool adaptive)
{
    static const uint8_t zero[BLOCK_BYTES] = {0};
    uint8_t mask[MASK_BYTES] = {0};
    size_t used = 0;

    for (size_t i = 0; i < BLOCKS; ++i) {
        if (memcmp(page + i * BLOCK_BYTES, zero, BLOCK_BYTES) != 0) {
            mask[i / 8] |= (uint8_t)(1u << (i % 8));
            ++used;
        }
    }

    size_t fallback_size = used == BLOCKS ? EXODUS_PAGE_BYTES : MASK_BYTES + used * BLOCK_BYTES;
    if (adaptive && used != 0) {
        size_t size = used == BLOCKS ? encode_repeated_tile(page, encoded) : 0;
        if (size == 0)
            size = encode_word_runs(page, encoded, fallback_size);
        if (size != 0)
            return size;
    }

    if (used == BLOCKS) {
        memcpy(encoded, page, EXODUS_PAGE_BYTES);
        return fallback_size;
    }

    memcpy(encoded, mask, MASK_BYTES);
    size_t offset = MASK_BYTES;
    for (size_t group = 0; group < MASK_BYTES; ++group) {
        unsigned bits = mask[group];
        const uint8_t *src = page + group * 8 * BLOCK_BYTES;
        if (bits == 0xffu) {
            memcpy(encoded + offset, src, 8 * BLOCK_BYTES);
            offset += 8 * BLOCK_BYTES;
        } else if (bits) {
            for (size_t i = 0; i < 8; ++i) {
                if (bits & (1u << i)) {
                    memcpy(encoded + offset, src + i * BLOCK_BYTES, BLOCK_BYTES);
                    offset += BLOCK_BYTES;
                }
            }
        }
    }
    return offset;
}

size_t exodus_page_encode(const uint8_t *page, uint8_t *encoded)
{
    return encode_page(page, encoded, false);
}


/* V3: version, eight-byte mask, then 64 blocks of either 20 or 64 bytes.
 * Base and words are explicitly little-endian. Deltas are unsigned bytes;
 * decoding uses defined modulo-2^32 arithmetic. No padding or block headers. */
enum { BDI_HEADER = 9, BDI_BYTES = 20 };
static uint32_t bdi_load(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 |
           (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void bdi_put(uint8_t *p, uint32_t v)
{
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i));
}
/* Fixed-cost byte population count; no table, alignment or ISA dependency. */
/* Read the full mask without alignment assumptions. The wire is little
 * endian; use a byte assembly fallback on unknown or big-endian targets. */
static uint64_t bdi_mask_load(const uint8_t *p)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint64_t v;
    memcpy(&v,p,sizeof v);
    return v;
#else
    uint64_t v=0;
    for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);
    return v;
#endif
}
static size_t bdi_popcount(uint64_t v)
{
    v-=(v>>1)&UINT64_C(0x5555555555555555);
    v=(v&UINT64_C(0x3333333333333333))+((v>>2)&UINT64_C(0x3333333333333333));
    v=(v+(v>>4))&UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (size_t)((v*UINT64_C(0x0101010101010101))>>56);
}
static size_t bdi_count(const uint8_t *mask,size_t limit)
{
    uint64_t v=bdi_mask_load(mask);
    if(limit<64)v&=(UINT64_C(1)<<limit)-1;
    return bdi_popcount(v);
}
static size_t encode_bdi(const uint8_t *page, uint8_t *encoded, size_t best)
{
    uint8_t mask[MASK_BYTES]={0};
    uint32_t bases[BLOCKS];
    /* Optimistic lower bound: every unexamined block compresses.
     * Each observed RAW block adds 44 bytes permanently. Ties keep the
     * existing format, so stop as soon as even this bound cannot win. */
    size_t size=BDI_HEADER+BLOCKS*BDI_BYTES;
    if(best<=size)return best;
    for(size_t b=0;b<BLOCKS;b++) {
        const uint8_t *p=page+b*BLOCK_BYTES;
        uint32_t lo=bdi_load(p),hi=lo;
        for(size_t w=1;w<BLOCK_BYTES/WORD_BYTES;w++) {
            uint32_t v=bdi_load(p+w*WORD_BYTES);
            if(v<lo) lo=v;
            if(v>hi) hi=v;
            if(hi-lo>255) break;
        }
        if(hi-lo<=255) { mask[b/8]|=(uint8_t)(1u<<(b%8)); bases[b]=lo; }
        else{
            size+=BLOCK_BYTES-BDI_BYTES;
            if(size>=best)return best;
        }
    }
    encoded[0]=EXODUS_PAGE_BDI_VERSION;
    memcpy(encoded+1,mask,MASK_BYTES);
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++) {
        const uint8_t *p=page+b*BLOCK_BYTES;
        if(mask[b/8]&(1u<<(b%8))) {
            bdi_put(encoded+pos,bases[b]);
            for(size_t w=0;w<16;w++) encoded[pos+4+w]=(uint8_t)(bdi_load(p+w*4)-bases[b]);
            pos+=BDI_BYTES;
        } else { memcpy(encoded+pos,p,BLOCK_BYTES);pos+=BLOCK_BYTES; }
    }
    return size;
}
static bool bdi_valid(const uint8_t *p,size_t size)
{
    return p && size>=BDI_HEADER && size<EXODUS_PAGE_BYTES &&
        p[0]==EXODUS_PAGE_BDI_VERSION &&
        size==BDI_HEADER+BLOCKS*BLOCK_BYTES-bdi_count(p+1,BLOCKS)*(BLOCK_BYTES-BDI_BYTES);
}
bool exodus_page_bdi_read_word(const uint8_t *p,size_t size,size_t word,uint8_t out[4])
{
    if(!out || word>=PAGE_WORDS || !p)return false;
    size_t b=word/16,w=word%16;
    /* The minimum BDI length uniquely implies 64 packed blocks. Validate
     * the entire mask before any payload access; memcpy is alignment-safe
     * and UINT64_MAX is independent of host byte order. No rank scan. */
    if(size==BDI_HEADER+BLOCKS*BDI_BYTES){
        uint64_t mask;
        memcpy(&mask,p+1,sizeof mask);
        if(p[0]!=EXODUS_PAGE_BDI_VERSION||mask!=UINT64_MAX)return false;
        size_t pos=BDI_HEADER+b*BDI_BYTES;
        bdi_put(out,bdi_load(p+pos)+p[pos+4+w]);
        return true;
    }
    if(size<BDI_HEADER||size>=EXODUS_PAGE_BYTES||p[0]!=EXODUS_PAGE_BDI_VERSION)return false;
    uint64_t mask=bdi_mask_load(p+1);
    if(size!=BDI_HEADER+BLOCKS*BLOCK_BYTES-bdi_popcount(mask)*(BLOCK_BYTES-BDI_BYTES))return false;
    size_t pos=BDI_HEADER+b*BLOCK_BYTES-
        bdi_popcount(mask&((UINT64_C(1)<<b)-1))*(BLOCK_BYTES-BDI_BYTES);
    if(p[1+b/8]&(1u<<(b%8))) bdi_put(out,bdi_load(p+pos)+p[pos+4+w]);
    else memcpy(out,p+pos+w*4,4);
    return true;
}

/* Decode only one block at a time; used for patching and logical equality. */
static void bdi_block(const uint8_t *p,bool compressed,uint8_t out[BLOCK_BYTES])
{
    if (!compressed) { memcpy(out,p,BLOCK_BYTES); return; }
    uint32_t base=bdi_load(p);
    for(size_t w=0;w<16;w++) bdi_put(out+w*4,base+p[4+w]);
}

/* Shared 64-byte block primitives for packed BDI and hot slabs. */
size_t exodus_bdi_block_encode(const uint8_t *block,uint8_t *out)
{
    uint32_t lo=bdi_load(block),hi=lo;
    for(size_t i=1;i<16;i++){
        uint32_t v=bdi_load(block+i*4);
        if(v<lo)lo=v;
        if(v>hi)hi=v;
        /* The observed range never shrinks. RAW is now inevitable. */
        if(hi-lo>255){
            if(out)memcpy(out,block,BLOCK_BYTES);
            return BLOCK_BYTES;
        }
    }
    /* A size-only preflight needs neither the base nor the deltas written. */
    if(out){
        bdi_put(out,lo);
        for(size_t i=0;i<16;i++)out[4+i]=(uint8_t)(bdi_load(block+i*4)-lo);
    }
    return BDI_BYTES;
}
bool exodus_bdi_block_decode(const uint8_t *encoded,size_t n,uint8_t *out)
{
    if(!encoded||!out||(n!=BDI_BYTES&&n!=BLOCK_BYTES))return false;
    bdi_block(encoded,n==BDI_BYTES,out);return true;
}
bool exodus_bdi_block_read(const uint8_t *encoded,size_t n,size_t word,uint8_t out[4])
{
    if(!encoded||!out||word>=16||(n!=BDI_BYTES&&n!=BLOCK_BYTES))return false;
    if(n==BDI_BYTES)bdi_put(out,bdi_load(encoded)+encoded[4+word]);
    else memcpy(out,encoded+4*word,4);
    return true;
}

/* Prepare one replacement block without materializing the encoded page. */
bool exodus_page_bdi_prepare_patch(const uint8_t *p,size_t size,size_t word,
                                   const uint8_t value[4],struct exodus_bdi_patch *out)
{
    if(!value||!out||word>=PAGE_WORDS||!bdi_valid(p,size))return false;
    size_t b=word/16,w=word%16;
    bool was=(p[1+b/8]&(1u<<(b%8)))!=0;
    struct exodus_bdi_patch patch={0};
    patch.offset=BDI_HEADER+b*BLOCK_BYTES-bdi_count(p+1,b)*(BLOCK_BYTES-BDI_BYTES);
    patch.old_len=was?BDI_BYTES:BLOCK_BYTES;
    const uint8_t *src=p+patch.offset;
    uint8_t delta;
    if(was&&exodus_bdi_direct_delta(src,w,value,&delta)){
        memcpy(patch.bytes,src,BDI_BYTES);
        patch.bytes[4+w]=delta;
        patch.new_len=BDI_BYTES;
        *out=patch;
        return true;
    }
    uint8_t block[BLOCK_BYTES];
    bdi_block(src,was,block);
    memcpy(block+w*4,value,4);
    patch.new_len=exodus_bdi_block_encode(block,patch.bytes);
    *out=patch;
    return true;
}

size_t exodus_page_bdi_patch_word(const uint8_t *p,size_t size,size_t word,
                                  const uint8_t value[4],uint8_t *out)
{
    struct exodus_bdi_patch patch;
    if(!out||!exodus_page_bdi_prepare_patch(p,size,word,value,&patch))return 0;
    size_t total=size-patch.old_len+patch.new_len;
    if(total>=EXODUS_PAGE_BYTES)return 0;
    size_t b=word/16,pos=patch.offset;
    uint8_t bit=(uint8_t)(1u<<(b%8));
    memcpy(out,p,pos);
    if(patch.new_len==BDI_BYTES)out[1+b/8]|=bit;
    else out[1+b/8]&=(uint8_t)~bit;
    memcpy(out+pos,patch.bytes,patch.new_len);
    memcpy(out+pos+patch.new_len,p+pos+patch.old_len,size-pos-patch.old_len);
    return total;
}


/* Position-dependent word tokens. XOR makes replacement reversible; the
 * SplitMix64 finalizer diffuses the combined index and little-endian word.
 * This is a candidate filter, never a proof of equality or integrity. */
static uint64_t word_token_value(size_t word,uint32_t value)
{
    uint64_t x=((uint64_t)word<<32)|value;
    x+=UINT64_C(0x9e3779b97f4a7c15);
    x=(x^(x>>30))*UINT64_C(0xbf58476d1ce4e5b9);
    x=(x^(x>>27))*UINT64_C(0x94d049bb133111eb);
    return x^(x>>31);
}
static uint64_t word_token(size_t word,const uint8_t value[4])
{
    return word_token_value(word,bdi_load(value));
}

uint64_t exodus_page_fingerprint(const uint8_t *page)
{
    uint64_t hash=UINT64_C(0x45584f4455530002);
    for(size_t w=0;w<PAGE_WORDS;w++)hash^=word_token(w,page+4*w);
    return hash;
}

bool exodus_page_fingerprint_update(uint64_t old,size_t word,
                                     const uint8_t before[4],const uint8_t after[4],
                                     uint64_t *out)
{
    if(!before||!after||!out||word>=PAGE_WORDS)return false;
    *out=old^word_token(word,before)^word_token(word,after);
    return true;
}

/* Preserve the original single hash loop for RAW-dominated records.
 * Only one 64-byte block is staged; no complete-page decode. */
static uint64_t bdi_fingerprint_staged(const uint8_t *p)
{
    uint64_t hash=UINT64_C(0x45584f4455530002);
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++){
        uint8_t block[BLOCK_BYTES];
        bool compressed=(p[1+b/8]&(1u<<(b%8)))!=0;
        bdi_block(p+pos,compressed,block);
        for(size_t w=0;w<16;w++)hash^=word_token(b*16+w,block+w*4);
        pos+=compressed?BDI_BYTES:BLOCK_BYTES;
    }
    return hash;
}
bool exodus_page_bdi_fingerprint(const uint8_t *p,size_t size,uint64_t *out)
{
    if(!out || !bdi_valid(p,size))return false;
    /* Validated size determines packed block count exactly. Select the
     * original loop for <=16 packed blocks; threshold is provisional. */
    if(size>=BDI_HEADER+BLOCKS*BLOCK_BYTES-16*(BLOCK_BYTES-BDI_BYTES)){
        *out=bdi_fingerprint_staged(p);
        return true;
    }
    uint64_t hash=UINT64_C(0x45584f4455530002);
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++){
        /* Hash logical words directly. Unsigned addition matches the
         * decoder's modulo-2^32 semantics even for noncanonical bases. */
        if(p[1+b/8]&(1u<<(b%8))){
            uint32_t base=bdi_load(p+pos);
            for(size_t w=0;w<16;w++)
                hash^=word_token_value(b*16+w,base+(uint32_t)p[pos+4+w]);
            pos+=BDI_BYTES;
        }else{
            /* Retain the original fixed-size RAW staging path: on host
             * compilers this enables vectorized token loads. Packed blocks
             * still hash without materialization. */
            uint8_t raw[BLOCK_BYTES];
            memcpy(raw,p+pos,sizeof raw);
            for(size_t w=0;w<16;w++)hash^=word_token(b*16+w,raw+4*w);
            pos+=BLOCK_BYTES;
        }
    }
    *out=hash;
    return true;
}

/* Caller validates the complete layout before inspecting any blocks.
 * Raw blocks compare in place; compressed blocks compare logical words.
 * Unsigned addition intentionally preserves the decoder's wrap semantics. */
static bool bdi_equal_validated(const uint8_t *p,const uint8_t *page)
{
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++){
        const uint8_t *logical=page+b*BLOCK_BYTES;
        if(p[1+b/8]&(1u<<(b%8))){
            uint32_t base=bdi_load(p+pos);
            for(size_t w=0;w<BLOCK_BYTES/WORD_BYTES;w++)
                if(base+(uint32_t)p[pos+WORD_BYTES+w]!=bdi_load(logical+w*WORD_BYTES))
                    return false;
            pos+=BDI_BYTES;
        }else{
            if(memcmp(p+pos,logical,BLOCK_BYTES)!=0)return false;
            pos+=BLOCK_BYTES;
        }
    }
    return true;
}
bool exodus_page_bdi_equal_page(const uint8_t *p,size_t size,const uint8_t *page)
{
    return page && bdi_valid(p,size) && bdi_equal_validated(p,page);
}

static bool decode_bdi(const uint8_t *p,size_t size,uint8_t *page)
{
    if(!bdi_valid(p,size)) return false;
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++) {
        uint8_t *out=page+b*BLOCK_BYTES;
        if(p[1+b/8]&(1u<<(b%8))) {
            uint32_t base=bdi_load(p+pos);
            for(size_t w=0;w<16;w++) bdi_put(out+w*4,base+p[pos+4+w]);
            pos+=BDI_BYTES;
        } else { memcpy(out,p+pos,BLOCK_BYTES);pos+=BLOCK_BYTES; }
    }
    return true;
}

size_t exodus_page_encode_adaptive(const uint8_t *page, uint8_t *encoded)
{
    return encode_bdi(page, encoded, encode_page(page, encoded, true));
}

static void restore_repeated_words(uint8_t *destination, const uint8_t *source, size_t count)
{
    uint32_t word;
    /* Keep the stored byte order and allow unaligned source and destination. */
    memcpy(&word, source, sizeof(word));
    const uint32_t block[] = {word, word, word, word};
    const size_t block_words = sizeof(block) / sizeof(word);
    while (count >= block_words) {
        memcpy(destination, block, sizeof(block));
        destination += sizeof(block);
        count -= block_words;
    }
    while (count > 0) {
        memcpy(destination, &word, sizeof(word));
        destination += sizeof(word);
        --count;
    }
}

static bool read_word_runs(const uint8_t *encoded, size_t encoded_size, uint8_t *page,
                           const uint8_t *compare, bool *match)
{
    if (encoded[0] != WORD_RUN_VERSION)
        return false;

    size_t offset = 1;
    for (size_t word = 0; word < PAGE_WORDS;) {
        if (offset == encoded_size)
            return false;
        uint8_t control = encoded[offset++];
        size_t count = (control & (WORD_RUN_MAX - 1)) + 1;
        bool repeated = (control & WORD_RUN_REPEAT) != 0;
        size_t payload_size = repeated ? WORD_BYTES : count * WORD_BYTES;
        if ((repeated && count < 2) || count > PAGE_WORDS - word ||
            payload_size > encoded_size - offset)
            return false;

        if (page) {
            uint8_t *destination = page + word * WORD_BYTES;
            if (repeated)
                restore_repeated_words(destination, encoded + offset, count);
            else
                memcpy(destination, encoded + offset, payload_size);
        }
        if (compare && *match) {
            const uint8_t *p=compare+word*WORD_BYTES;
            if(repeated){
                /* A matching first word and adjacent repetition prove the run. */
                if(memcmp(p,encoded+offset,WORD_BYTES) ||
                   memcmp(p,p+WORD_BYTES,(count-1)*WORD_BYTES)) *match=false;
            }else if(memcmp(p,encoded+offset,payload_size)) *match=false;
        }
        offset += payload_size;
        word += count;
    }

    return offset == encoded_size ||
           (offset % 2 == 0 && offset + 1 == encoded_size && encoded[offset] == 0);
}

bool exodus_page_decode(const uint8_t *encoded, size_t encoded_size, uint8_t *page)
{
    if (encoded_size > EXODUS_PAGE_BYTES)
        return false;
    if (encoded_size == EXODUS_PAGE_BYTES) {
        memcpy(page, encoded, EXODUS_PAGE_BYTES);
        return true;
    }
    if (encoded_size % 2 != 0) {
        if (encoded[0] == EXODUS_PAGE_BDI_VERSION)
            return decode_bdi(encoded, encoded_size, page);
        if (encoded[0] == EXODUS_PAGE_REPEATED_TILE_VERSION) {
            size_t tile_size = encoded_size - 1;
            if (tile_size != 8 && tile_size != 16)
                return false;
            memcpy(page, encoded + 1, tile_size);
            for (size_t filled = tile_size; filled < EXODUS_PAGE_BYTES; filled *= 2)
                memcpy(page + filled, page, filled);
            return true;
        }
        /* Validate all commands and padding before touching the output page. */
        return read_word_runs(encoded, encoded_size, NULL, NULL, NULL) &&
               read_word_runs(encoded, encoded_size, page, NULL, NULL);
    }
    if (encoded_size <= MASK_BYTES) {
        static const uint8_t zero_mask[MASK_BYTES] = {0};
        if (encoded_size != MASK_BYTES || memcmp(encoded, zero_mask, MASK_BYTES) != 0)
            return false;
        memset(page, 0, EXODUS_PAGE_BYTES);
        return true;
    }
    if ((encoded_size - MASK_BYTES) % BLOCK_BYTES != 0)
        return false;

    size_t expected = MASK_BYTES;
    for (size_t i = 0; i < MASK_BYTES; ++i) {
        unsigned bits = encoded[i];
        while (bits != 0) {
            bits &= bits - 1;
            expected += BLOCK_BYTES;
        }
    }
    if (expected != encoded_size) {
        return false;
    }

    size_t offset = MASK_BYTES;
    for (size_t group = 0; group < MASK_BYTES; ++group) {
        unsigned bits = encoded[group];
        uint8_t *dst = page + group * 8 * BLOCK_BYTES;
        if (bits == 0xffu) {
            memcpy(dst, encoded + offset, 8 * BLOCK_BYTES);
            offset += 8 * BLOCK_BYTES;
        } else if (!bits) {
            memset(dst, 0, 8 * BLOCK_BYTES);
        } else {
            for (size_t i = 0; i < 8; ++i) {
                uint8_t *block = dst + i * BLOCK_BYTES;
                if (bits & (1u << i)) {
                    memcpy(block, encoded + offset, BLOCK_BYTES);
                    offset += BLOCK_BYTES;
                } else {
                    memset(block, 0, BLOCK_BYTES);
                }
            }
        }
    }
    return true;
}

/* Exact comparison, retaining the decoder's validation rules and format
 * precedence. A mismatch never suppresses validation of a later run. */
bool exodus_page_compare(const uint8_t *p,size_t size,const uint8_t *page,bool *equal)
{
    if(!p||!page||!equal||!size||size>EXODUS_PAGE_BYTES)return false;
    bool match=true;
    if(size==EXODUS_PAGE_BYTES){
        match=memcmp(p,page,size)==0;
    }else if(size&1u){
        if(p[0]==EXODUS_PAGE_BDI_VERSION){
            if(!bdi_valid(p,size))return false;
            match=bdi_equal_validated(p,page);
        }else if(p[0]==EXODUS_PAGE_REPEATED_TILE_VERSION){
            size_t tile=size-1;
            if(tile!=8&&tile!=16)return false;
            match=!memcmp(p+1,page,tile)&&!memcmp(page,page+tile,EXODUS_PAGE_BYTES-tile);
        }else if(!read_word_runs(p,size,NULL,page,&match))return false;
    }else{
        static const uint8_t zero[BLOCK_BYTES]={0};
        if(size<MASK_BYTES||size!=MASK_BYTES+bdi_count(p,BLOCKS)*BLOCK_BYTES)return false;
        size_t pos=MASK_BYTES;
        for(size_t b=0;b<BLOCKS;b++){
            bool present=(p[b/8]&(1u<<(b%8)))!=0;
            if(memcmp(present?p+pos:zero,page+b*BLOCK_BYTES,BLOCK_BYTES)){
                match=false;break;
            }
            if(present)pos+=BLOCK_BYTES;
        }
    }
    *equal=match;
    return true;
}
