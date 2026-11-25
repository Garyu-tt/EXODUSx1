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
    for (size_t i = 0; i < BLOCKS; ++i) {
        if (mask[i / 8] & (1u << (i % 8))) {
            memcpy(encoded + offset, page + i * BLOCK_BYTES, BLOCK_BYTES);
            offset += BLOCK_BYTES;
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
static size_t bdi_count(const uint8_t *mask, size_t limit)
{
    size_t n=0;
    for (size_t i=0;i<limit/8;i++) {
        unsigned b=mask[i];
        while(b) { b &= b-1; ++n; }
    }
    for (size_t i=limit/8*8;i<limit;i++)
        n += (mask[i/8]>>(i%8))&1u;
    return n;
}
static size_t encode_bdi(const uint8_t *page, uint8_t *encoded, size_t best)
{
    uint8_t mask[MASK_BYTES]={0};
    uint32_t bases[BLOCKS];
    /* Even a fully compressed BDI page cannot beat a smaller existing form. */
    if (best <= BDI_HEADER+BLOCKS*BDI_BYTES) return best;
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
    }
    size_t n=bdi_count(mask,BLOCKS);
    size_t size=BDI_HEADER+BLOCKS*BLOCK_BYTES-n*(BLOCK_BYTES-BDI_BYTES);
    if(size>=best) return best;
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
    if(!out || word>=PAGE_WORDS || !bdi_valid(p,size)) return false;
    size_t b=word/16,w=word%16;
    size_t pos=BDI_HEADER+b*BLOCK_BYTES-bdi_count(p+1,b)*(BLOCK_BYTES-BDI_BYTES);
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
    uint8_t block[BLOCK_BYTES];
    bdi_block(p+patch.offset,was,block);
    memcpy(block+w*4,value,4);
    uint32_t lo=bdi_load(block),hi=lo;
    for(size_t i=1;i<16;i++){
        uint32_t v=bdi_load(block+i*4);
        if(v<lo)lo=v;
        if(v>hi)hi=v;
    }
    bool fits=hi-lo<=255;
    patch.new_len=fits?BDI_BYTES:BLOCK_BYTES;
    if(fits){
        bdi_put(patch.bytes,lo);
        for(size_t i=0;i<16;i++)patch.bytes[4+i]=(uint8_t)(bdi_load(block+i*4)-lo);
    }else memcpy(patch.bytes,block,BLOCK_BYTES);
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
static uint64_t word_token(size_t word,const uint8_t value[4])
{
    uint64_t x=((uint64_t)word<<32)|bdi_load(value);
    x+=UINT64_C(0x9e3779b97f4a7c15);
    x=(x^(x>>30))*UINT64_C(0xbf58476d1ce4e5b9);
    x=(x^(x>>27))*UINT64_C(0x94d049bb133111eb);
    return x^(x>>31);
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

bool exodus_page_bdi_fingerprint(const uint8_t *p,size_t size,uint64_t *out)
{
    if(!out || !bdi_valid(p,size))return false;
    uint64_t hash=UINT64_C(0x45584f4455530002);
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++){
        uint8_t block[BLOCK_BYTES];
        bool compressed=(p[1+b/8]&(1u<<(b%8)))!=0;
        bdi_block(p+pos,compressed,block);
        for(size_t w=0;w<16;w++)hash^=word_token(b*16+w,block+w*4);
        pos+=compressed?BDI_BYTES:BLOCK_BYTES;
    }
    *out=hash;
    return true;
}

bool exodus_page_bdi_equal_page(const uint8_t *p,size_t size,const uint8_t *page)
{
    if(!page || !bdi_valid(p,size))return false;
    size_t pos=BDI_HEADER;
    for(size_t b=0;b<BLOCKS;b++){
        uint8_t block[BLOCK_BYTES];
        bool compressed=(p[1+b/8]&(1u<<(b%8)))!=0;
        bdi_block(p+pos,compressed,block);
        if(memcmp(block,page+b*BLOCK_BYTES,BLOCK_BYTES)!=0)return false;
        pos+=compressed?BDI_BYTES:BLOCK_BYTES;
    }
    return true;
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

static bool read_word_runs(const uint8_t *encoded, size_t encoded_size, uint8_t *page)
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
        return read_word_runs(encoded, encoded_size, NULL) &&
               read_word_runs(encoded, encoded_size, page);
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
    for (size_t i = 0; i < BLOCKS; ++i) {
        uint8_t *block = page + i * BLOCK_BYTES;
        if (encoded[i / 8] & (1u << (i % 8))) {
            memcpy(block, encoded + offset, BLOCK_BYTES);
            offset += BLOCK_BYTES;
        } else {
            memset(block, 0, BLOCK_BYTES);
        }
    }
    return true;
}
