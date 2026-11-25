#include "bdi_slab.h"
#include <string.h>

enum { RAW_BIT=32768, INDEX_MASK=32767, SMALL=20, RAW=64 };
static bool valid(const struct exodus_bdi_slab *s)
{
    return s && s->small_capacity<=EXODUS_BDI_SLAB_LIMIT && s->raw_capacity<=EXODUS_BDI_SLAB_LIMIT &&
        (!s->small_capacity||(s->small&&s->small_refs)) && (!s->raw_capacity||(s->raw&&s->raw_refs));
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
/* Reserve IDs without touching the actual pool: failed imports are atomic. */
static bool reserve(const struct exodus_bdi_slab *s,bool raw,size_t *cursor,uint16_t *id)
{
    size_t i=*cursor;
    while(i<capacity(s,raw)&&refs(s,raw)[i])++i;
    if(i==capacity(s,raw))return false;
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
        memcpy(slot(s,ids[i]),encoded,n);*ref(s,ids[i])=1;
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
    for(size_t i=0;i<64;i++)--*ref(s,p->ids[i]);
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
    memcpy(slot(s,id),encoded,n);*ref(s,id)=1;--*ref(s,old);p->ids[b]=id;return true;
}
