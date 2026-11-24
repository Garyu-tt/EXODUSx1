/* Local library state, version 1: two independent checksummed snapshots. */
#ifndef EXODUS_LIBRARY_STATE_H
#define EXODUS_LIBRARY_STATE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#define EXODUS_STATE_MAX 4096
#define EXODUS_STATE_ID_MAX 511
struct exodus_state_row {
    char provider[16], id[EXODUS_STATE_ID_MAX+1];
    uint64_t requested;
    unsigned char favorite;
};
struct exodus_state {
    struct exodus_state_row *rows;
    size_t count;
    uint64_t generation;
    int slot, writable;
};
static inline void exodus_state_clear(struct exodus_state *s)
{
    free(s->rows);
    memset(s,0,sizeof(*s));
    s->slot=-1;
}
static inline int exodus_state_key(const char *p,const char *id)
{
    return p && id && *p && *id && strlen(p)<16 && strlen(id)<=EXODUS_STATE_ID_MAX;
}
static inline struct exodus_state_row *exodus_state_find(const struct exodus_state *s,const char *p,const char *id)
{
    for(size_t i=0;i<s->count;i++)
        if(!strcmp(s->rows[i].provider,p)&&!strcmp(s->rows[i].id,id)) return &s->rows[i];
    return NULL;
}
static inline struct exodus_state_row *exodus_state_get(struct exodus_state *s,const char *p,const char *id)
{
    if(!exodus_state_key(p,id)) return NULL;
    struct exodus_state_row *r=exodus_state_find(s,p,id);
    if(r) return r;
    if(s->count>=EXODUS_STATE_MAX) return NULL;
    r=realloc(s->rows,(s->count+1)*sizeof(*r));
    if(!r) return NULL;
    s->rows=r;
    r=&s->rows[s->count++];
    memset(r,0,sizeof(*r));
    strcpy(r->provider,p); strcpy(r->id,id);
    return r;
}
static inline uint32_t exodus_state_crc(uint32_t c,const void *bytes,size_t n)
{
    const unsigned char *p=bytes;
    while(n--) {
        c^=*p++;
        for(int i=0;i<8;i++) c=(c>>1)^((0u-(c&1u))&0xedb88320u);
    }
    return c;
}
static inline void exodus_state_pack(unsigned char *p,uint64_t n,size_t bytes)
{
    for(size_t i=0;i<bytes;i++) p[i]=(unsigned char)(n>>(8*i));
}
static inline uint64_t exodus_state_unpack(const unsigned char *p,size_t bytes)
{
    uint64_t n=0;
    for(size_t i=0;i<bytes;i++) n|=(uint64_t)p[i]<<(8*i);
    return n;
}
static inline int exodus_state_io(FILE *f,void *p,size_t n,uint32_t *crc,int writing)
{
    size_t actual=writing?fwrite(p,1,n,f):fread(p,1,n,f);
    if(actual!=n) return 0;
    *crc=exodus_state_crc(*crc,p,n);
    return 1;
}
static inline int exodus_state_write(FILE *f,const struct exodus_state *s,uint64_t generation)
{
    unsigned char h[16]={ 'E','X','S','1' }, meta[12], crcbytes[4];
    uint32_t crc=0xffffffffu;
    if(s->count>EXODUS_STATE_MAX || !generation) return 0;
    exodus_state_pack(h+4,generation,8); exodus_state_pack(h+12,s->count,4);
    if(!exodus_state_io(f,h,sizeof(h),&crc,1)) return 0;
    for(size_t i=0;i<s->count;i++){
        const struct exodus_state_row *r=&s->rows[i];
        if(!exodus_state_key(r->provider,r->id) || r->favorite>1) return 0;
        size_t pn=strlen(r->provider), in=strlen(r->id);
        meta[0]=(unsigned char)pn; exodus_state_pack(meta+1,in,2);
        meta[3]=r->favorite; exodus_state_pack(meta+4,r->requested,8);
        if(!exodus_state_io(f,meta,sizeof(meta),&crc,1) ||
            !exodus_state_io(f,(void *)r->provider,pn,&crc,1) ||
            !exodus_state_io(f,(void *)r->id,in,&crc,1)) return 0;
    }
    exodus_state_pack(crcbytes,crc^0xffffffffu,4);
    return fwrite(crcbytes,1,4,f)==4;
}
static inline int exodus_state_read(FILE *f,struct exodus_state *s)
{
    unsigned char h[16],meta[12],crcbytes[4];
    uint32_t crc=0xffffffffu;
    struct exodus_state tmp={0}; tmp.slot=-1;
    if(!exodus_state_io(f,h,sizeof(h),&crc,0) || memcmp(h,"EXS1",4)) goto bad;
    tmp.generation=exodus_state_unpack(h+4,8);
    uint64_t count=exodus_state_unpack(h+12,4);
    if(!tmp.generation || count>EXODUS_STATE_MAX) goto bad;
    for(uint64_t i=0;i<count;i++){
        char provider[16]={0},id[EXODUS_STATE_ID_MAX+1]={0};
        if(!exodus_state_io(f,meta,sizeof(meta),&crc,0)) goto bad;
        size_t pn=meta[0],in=(size_t)exodus_state_unpack(meta+1,2);
        if(!pn || pn>=sizeof(provider) || !in || in>=sizeof(id) || meta[3]>1) goto bad;
        if(!exodus_state_io(f,provider,pn,&crc,0) || !exodus_state_io(f,id,in,&crc,0) ||
            memchr(provider,0,pn) || memchr(id,0,in) || exodus_state_find(&tmp,provider,id)) goto bad;
        struct exodus_state_row *r=exodus_state_get(&tmp,provider,id);
        if(!r) goto bad;
        r->favorite=meta[3]; r->requested=exodus_state_unpack(meta+4,8);
    }
    if(fread(crcbytes,1,4,f)!=4 || exodus_state_unpack(crcbytes,4)!=(crc^0xffffffffu) ||
        fgetc(f)!=EOF || ferror(f)) goto bad;
    exodus_state_clear(s); *s=tmp; return 1;
bad:
    exodus_state_clear(&tmp); return 0;
}
/* Missing is distinct from corrupt/unreadable. Never overwrite two bad snapshots. */
static inline int exodus_state_read_slot(const char *path,struct exodus_state *s)
{
    FILE *f=fopen(path,"rb");
    if(!f) return errno==ENOENT?0:-1;
    int ok=exodus_state_read(f,s);
    if(fclose(f)) ok=0;
    return ok?1:-1;
}
static inline void exodus_state_open(struct exodus_state *s,const char *a,const char *b)
{
    struct exodus_state x={0},y={0}; x.slot=y.slot=-1;
    int ra=exodus_state_read_slot(a,&x),rb=exodus_state_read_slot(b,&y);
    exodus_state_clear(s);
    if(ra==1 && (rb!=1 || x.generation>=y.generation)) {
        *s=x; s->slot=0; memset(&x,0,sizeof(x));
    } else if(rb==1) {
        *s=y; s->slot=1; memset(&y,0,sizeof(y));
    }
    s->writable=ra==1 || rb==1 || (ra==0 && rb==0);
    exodus_state_clear(&x); exodus_state_clear(&y);
}
static inline int exodus_state_save(struct exodus_state *s,const char *a,const char *b)
{
    if(!s->writable || s->generation==UINT64_MAX) return 0;
    int next=s->slot==0?1:0;
    FILE *f=fopen(next?b:a,"wb");
    if(!f) return 0;
    int ok=exodus_state_write(f,s,s->generation+1) && !fflush(f) && !fsync(fileno(f));
    if(fclose(f)) ok=0;
    if(ok){s->slot=next;s->generation++;}
    return ok;
}
#endif
