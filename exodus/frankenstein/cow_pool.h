#ifndef EXODUS_COW_POOL_H
#define EXODUS_COW_POOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Serialized, fixed-capacity allocator over caller-owned backing.
 * Handles are indices, never pointers from guest requests. This model
 * is not a live page table manager; caller must quiesce mappings and
 * complete BBM/TLB invalidation before releasing old backing. */
enum { CP_PAGE=4096, CP_MAX=32, CP_TASKS=8, CP_VPAGES=16, CP_NONE=255 };
struct cp_pool {
    uint8_t *bytes;
    unsigned count;
    uint16_t refs[CP_MAX];
    uint8_t live[CP_TASKS], map[CP_TASKS][CP_VPAGES];
    uint64_t copies, clears;
};
static inline int cp_init(struct cp_pool *p,uint8_t *bytes,size_t len,unsigned n){
    if(!p||!bytes||!n||n>CP_MAX||len<(size_t)n*CP_PAGE)return 0;
    memset(p,0,sizeof *p);p->bytes=bytes;p->count=n;
    memset(p->map,CP_NONE,sizeof p->map);return 1;
}
static inline int cp_valid(const struct cp_pool *p){
    if(!p||!p->bytes||!p->count||p->count>CP_MAX)return 0;
    uint16_t actual[CP_MAX]={0};
    for(unsigned t=0;t<CP_TASKS;t++){
        if(p->live[t]>1)return 0;
        for(unsigned v=0;v<CP_VPAGES;v++){
            unsigned f=p->map[t][v];
            if(f==CP_NONE)continue;
            if(!p->live[t]||f>=p->count)return 0;
            actual[f]++;
        }
    }
    for(unsigned f=0;f<CP_MAX;f++)if(actual[f]!=p->refs[f])return 0;
    return 1;
}
static inline int cp_create(struct cp_pool *p,unsigned t){
    if(!cp_valid(p)||t>=CP_TASKS||p->live[t])return 0;
    p->live[t]=1;return 1;
}
static inline int cp_free_frame(const struct cp_pool *p){
    for(unsigned i=0;i<p->count;i++)if(!p->refs[i])return (int)i;
    return -1;
}
static inline int cp_alloc(struct cp_pool *p,unsigned t,unsigned v){
    if(!cp_valid(p)||t>=CP_TASKS||v>=CP_VPAGES||!p->live[t]||p->map[t][v]!=CP_NONE)return 0;
    int f=cp_free_frame(p);if(f<0)return 0;
    memset(p->bytes+(size_t)f*CP_PAGE,0,CP_PAGE);
    p->clears++;p->refs[f]=1;p->map[t][v]=(uint8_t)f;return 1;
}
static inline int cp_clone(struct cp_pool *p,unsigned parent,unsigned child){
    if(!cp_valid(p)||parent>=CP_TASKS||child>=CP_TASKS||!p->live[parent]||p->live[child])return 0;
    /* Bounded mapping count guarantees refcount cannot overflow. */
    for(unsigned v=0;v<CP_VPAGES;v++){
        unsigned f=p->map[parent][v];
        if(f!=CP_NONE){p->map[child][v]=(uint8_t)f;p->refs[f]++;}
    }
    p->live[child]=1;return 1;
}
static inline int cp_split(struct cp_pool *p,unsigned t,unsigned v,unsigned *frame){
    if(!frame||!cp_valid(p)||t>=CP_TASKS||v>=CP_VPAGES||!p->live[t])return 0;
    unsigned old=p->map[t][v];if(old==CP_NONE)return 0;
    if(p->refs[old]==1){*frame=old;return 1;}
    int next=cp_free_frame(p);if(next<0)return 0; /* no partial mutation */
    memcpy(p->bytes+(size_t)next*CP_PAGE,p->bytes+(size_t)old*CP_PAGE,CP_PAGE);
    p->copies++;p->refs[next]=1;p->refs[old]--;p->map[t][v]=(uint8_t)next;
    *frame=(unsigned)next;return 1;
}
static inline int cp_drop(struct cp_pool *p,unsigned t){
    if(!cp_valid(p)||t>=CP_TASKS||!p->live[t])return 0;
    for(unsigned v=0;v<CP_VPAGES;v++){
        unsigned f=p->map[t][v];
        if(f!=CP_NONE){p->refs[f]--;p->map[t][v]=CP_NONE;}
    }
    p->live[t]=0;return 1;
}
static inline unsigned cp_used(const struct cp_pool *p){
    unsigned n=0;for(unsigned i=0;i<p->count;i++)n+=p->refs[i]!=0;return n;
}
#endif
