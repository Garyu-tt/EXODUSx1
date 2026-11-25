#include "bdi_slab.h"
#include <string.h>

enum { RAW_BIT=32768, INDEX_MASK=32767, SMALL=20, RAW=64 };
static bool valid(const struct exodus_bdi_slab *s)
{
    return s && s->small_capacity<=EXODUS_BDI_SLAB_LIMIT && s->raw_capacity<=EXODUS_BDI_SLAB_LIMIT &&
        (!s->small_capacity||(s->small&&s->small_refs)) && (!s->raw_capacity||(s->raw&&s->raw_refs)) &&
        (!s->free_index||(s->free_index->cls[0].capacity==s->small_capacity&&s->free_index->cls[1].capacity==s->raw_capacity));
}
static size_t capacity(const struct exodus_bdi_slab *s,bool raw){return raw?s->raw_capacity:s->small_capacity;}
static uint16_t *refs(const struct exodus_bdi_slab *s,bool raw){return raw?s->raw_refs:s->small_refs;}
static size_t bytes(uint16_t id){return id&RAW_BIT?RAW:SMALL;}
static uint8_t *slot(const struct exodus_bdi_slab *s,uint16_t id){return (id&RAW_BIT?s->raw:s->small)+(id&INDEX_MASK)*bytes(id);}
static uint16_t *ref(const struct exodus_bdi_slab *s,uint16_t id){return refs(s,(id&RAW_BIT)!=0)+(id&INDEX_MASK);}
static bool live(const struct exodus_bdi_slab *s,uint16_t id){return (size_t)(id&INDEX_MASK)<capacity(s,(id&RAW_BIT)!=0)&&*ref(s,id)>0;}
static bool page_ok(const struct exodus_bdi_slab *s,const struct exodus_bdi_slab_page *p)
{
    if(!valid(s)||!p||!p->active)return false;
    for(size_t i=0;i<64;i++){
        if(!live(s,p->ids[i]))return false;
        for(size_t j=0;j<i;j++)if(p->ids[j]==p->ids[i])return false;
    }
    return true;
}

static unsigned first_bit(uint64_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned)__builtin_ctzll(v);
#else
    unsigned n=0;while(!(v&1u)){v>>=1;++n;}return n;
#endif
}
static uint64_t bit(unsigned i){return UINT64_C(1)<<i;}
static void mark(struct exodus_bdi_free_class *c,size_t i,bool free)
{
    size_t w=i/64,g=w/64;
    if(free)c->bits[w]|=bit(i%64);else c->bits[w]&=~bit(i%64);
    if(c->bits[w])c->summary[g]|=bit(w%64);else c->summary[g]&=~bit(w%64);
    if(c->summary[g])c->top|=(uint8_t)bit(g);else c->top&=(uint8_t)~bit(g);
}
bool exodus_bdi_slab_attach_index(struct exodus_bdi_slab *s,struct exodus_bdi_free_index *idx)
{
    if(!idx||!valid(s)||s->free_index)return false;
    memset(idx,0,sizeof *idx);
    for(unsigned k=0;k<2;k++){
        struct exodus_bdi_free_class *c=&idx->cls[k];
        c->capacity=capacity(s,k!=0);
        for(size_t i=0;i<c->capacity;i++)if(!refs(s,k!=0)[i])mark(c,i,true);
    }
    s->free_index=idx;return true;
}
void exodus_bdi_slab_detach_index(struct exodus_bdi_slab *s)
{ if(s)s->free_index=NULL; }
bool exodus_bdi_slab_check_index(const struct exodus_bdi_slab *s)
{
    if(!valid(s)||!s->free_index)return false;
    for(unsigned k=0;k<2;k++){
        const struct exodus_bdi_free_class *c=&s->free_index->cls[k];
        uint8_t top=0;
        for(size_t g=0;g<8;g++){
            uint64_t sum=0;
            for(size_t w=g*64;w<(g+1)*64;w++){
                uint64_t expected=0;
                for(size_t i=w*64;i<(w+1)*64&&i<c->capacity;i++)
                    if(!refs(s,k!=0)[i])expected|=bit(i%64);
                if(c->bits[w]!=expected)return false;
                if(expected)sum|=bit(w%64);
            }
            if(c->summary[g]!=sum)return false;
            if(sum)top|=(uint8_t)bit(g);
        }
        if(c->top!=top)return false;
    }
    return true;
}
/* Nonmutating search at or after cursor. At most one leaf, one summary
 * and the top bitmap are inspected before descending to the next leaf. */
static bool indexed_next(const struct exodus_bdi_free_class *c,size_t cursor,size_t *out)
{
    if(cursor>=c->capacity)return false;
    size_t w=cursor/64,g=w/64;
    uint64_t free=c->bits[w]&(~UINT64_C(0)<<(cursor%64));
    if(!free){
        uint64_t later=c->summary[g]&(~UINT64_C(0)<<(w%64));
        later&=~bit(w%64);
        if(!later){
            unsigned groups=(unsigned)c->top&(~0u<<(unsigned)g)&~(1u<<(unsigned)g);
            if(!groups)return false;
            g=first_bit(groups);
            later=c->summary[g];
            if(!later)return false;
        }
        w=g*64+first_bit(later);
        free=c->bits[w];
        if(!free)return false;
    }
    size_t i=w*64+first_bit(free);
    if(i>=c->capacity)return false;
    *out=i;return true;
}
static void occupy(struct exodus_bdi_slab *s,uint16_t id)
{
    *ref(s,id)=1;
    if(s->free_index)mark(&s->free_index->cls[(id&RAW_BIT)!=0],id&INDEX_MASK,false);
}
static void drop(struct exodus_bdi_slab *s,uint16_t id)
{
    if(--*ref(s,id)==0&&s->free_index)
        mark(&s->free_index->cls[(id&RAW_BIT)!=0],id&INDEX_MASK,true);
}

/* Reserve IDs without touching the actual pool: failed imports are atomic. */
static bool reserve(const struct exodus_bdi_slab *s,bool raw,size_t *cursor,uint16_t *id)
{
    size_t i=*cursor;
    if(s->free_index){
        if(!indexed_next(&s->free_index->cls[raw],i,&i)||refs(s,raw)[i])return false;
    }else{
        while(i<capacity(s,raw)&&refs(s,raw)[i])++i;
        if(i==capacity(s,raw))return false;
    }
    *id=(uint16_t)(i|(raw?RAW_BIT:0));*cursor=i+1;return true;
}
bool exodus_bdi_slab_import(struct exodus_bdi_slab *s,struct exodus_bdi_slab_page *p,const uint8_t *page)
{
    if(!valid(s)||!p||p->active||!page)return false;
    uint16_t ids[64];size_t cursors[2]={0};uint8_t encoded[64];
    for(size_t i=0;i<64;i++){
        bool raw=exodus_bdi_block_encode(page+i*64,encoded)==RAW;
        if(!reserve(s,raw,&cursors[raw],&ids[i]))return false;
    }
    for(size_t i=0;i<64;i++){
        size_t n=exodus_bdi_block_encode(page+i*64,encoded);
        memcpy(slot(s,ids[i]),encoded,n);occupy(s,ids[i]);
    }
    memcpy(p->ids,ids,sizeof ids);p->active=true;return true;
}
bool exodus_bdi_slab_clone(struct exodus_bdi_slab *s,struct exodus_bdi_slab_page *dst,const struct exodus_bdi_slab_page *src)
{
    if(!dst||dst->active||!page_ok(s,src))return false;
    for(size_t i=0;i<64;i++)if(*ref(s,src->ids[i])==UINT16_MAX)return false;
    for(size_t i=0;i<64;i++)++*ref(s,src->ids[i]);
    *dst=*src;return true;
}
bool exodus_bdi_slab_release(struct exodus_bdi_slab *s,struct exodus_bdi_slab_page *p)
{
    if(!page_ok(s,p))return false;
    for(size_t i=0;i<64;i++)drop(s,p->ids[i]);
    memset(p,0,sizeof *p);return true;
}
bool exodus_bdi_slab_restore(const struct exodus_bdi_slab *s,const struct exodus_bdi_slab_page *p,uint8_t *page)
{
    if(!page||!page_ok(s,p))return false;
    for(size_t i=0;i<64;i++)exodus_bdi_block_decode(slot(s,p->ids[i]),bytes(p->ids[i]),page+i*64);
    return true;
}
bool exodus_bdi_slab_read(const struct exodus_bdi_slab *s,const struct exodus_bdi_slab_page *p,size_t word,uint8_t out[4])
{
    if(!out||!valid(s)||!p||!p->active||word>=1024)return false;
    uint16_t id=p->ids[word/16];if(!live(s,id))return false;
    return exodus_bdi_block_read(slot(s,id),bytes(id),word%16,out);
}
bool exodus_bdi_slab_write(struct exodus_bdi_slab *s,struct exodus_bdi_slab_page *p,size_t word,const uint8_t value[4])
{
    if(!value||word>=1024||!page_ok(s,p))return false;
    size_t b=word/16,w=word%16;uint16_t old=p->ids[b];uint8_t block[64],encoded[64];
    exodus_bdi_block_decode(slot(s,old),bytes(old),block);
    if(!memcmp(block+w*4,value,4))return true;
    memcpy(block+w*4,value,4);size_t n=exodus_bdi_block_encode(block,encoded);
    if(n==bytes(old)&&*ref(s,old)==1){memcpy(slot(s,old),encoded,n);return true;}
    size_t cursor=0;uint16_t id;
    if(!reserve(s,n==RAW,&cursor,&id))return false;
    memcpy(slot(s,id),encoded,n);occupy(s,id);drop(s,old);p->ids[b]=id;return true;
}
