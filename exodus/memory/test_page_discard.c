#include "page_dedup_store.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

enum { P=EXODUS_PAGE_BYTES, N=16 };
struct trace { uint8_t *base; size_t granule,capacity,calls,bytes,fail_at; const struct exodus_page_store *store; };
static bool trace_discard(void *opaque,void *address,size_t n)
{
    struct trace *t=opaque;
    size_t a=(uint8_t*)address-t->base;
    assert(a%t->granule==0&&n%t->granule==0&&a<=t->capacity&&n<=t->capacity-a);
    for(size_t i=0;i<t->store->page_count;i++){
        size_t x,y;assert(exodus_page_store_extent(t->store,i,&x,&y));
        assert(a+n<=x||a>=y);
    }
    if(++t->calls==t->fail_at)return false;
    memset(address,0,n);t->bytes+=n;return true;
}
static void portable(void)
{
    uint8_t memory[16*P+P],enc[P];size_t starts[N]={0},ends[N]={0};uint32_t order[N]={0};
    uint8_t *base=memory+(P-(uintptr_t)memory%P)%P;
    struct exodus_page_store s={.payload=base,.payload_capacity=16*P,.page_ends=ends,.page_capacity=N,.page_starts=starts,.page_order=order};
    memset(base,0xa5,16*P);memset(enc,0x33,P);
    for(size_t i=0;i<8;i++)assert(exodus_page_store_pool_put(&s,i,enc,P));
    assert(exodus_page_store_pool_remove(&s,6));assert(exodus_page_store_pool_remove(&s,3));
    struct trace t={base,P,16*P,0,0,2,&s};size_t done=123;
    assert(!exodus_page_store_pool_discard_free(&s,P,16*P,trace_discard,&t,&done)&&done==P&&t.bytes==P&&t.calls==2);
    t.fail_at=0;t.calls=t.bytes=0;
    assert(exodus_page_store_pool_discard_free(&s,P,2*P+1,trace_discard,&t,&done)&&done==2*P&&t.calls==2);
    for(size_t i=0;i<s.page_count;i++)assert(!memcmp(base+starts[i],enc,P));
    t.calls=0;done=777;assert(!exodus_page_store_pool_discard_free(&s,0,P,trace_discard,&t,&done)&&done==777&&!t.calls);
    order[1]=order[0];assert(!exodus_page_store_pool_discard_free(&s,P,P,trace_discard,&t,&done)&&done==777&&!t.calls);
    order[1]=1;assert(exodus_page_store_validate(&s));
    /* Free partial granules cannot be discarded. */
    assert(exodus_page_store_pool_put(&s,0,enc,P-1));
    t.calls=t.bytes=0;assert(exodus_page_store_pool_discard_free(&s,P,16*P,trace_discard,&t,&done)&&done==10*P);
    assert(exodus_page_store_pool_put(&s,s.page_count,enc,P));
    assert(!memcmp(base+starts[s.page_count-1],enc,P));
    puts("portable discard: alignment, partial failure, budget, corrupted index, live bytes and reuse passed");
}
#if defined(__linux__)
static bool linux_discard(void *context,void *address,size_t bytes){(void)context;return madvise(address,bytes,MADV_DONTNEED)==0;}
static size_t resident(uint8_t *p,size_t bytes,size_t granule){unsigned char vec[64];size_t n=bytes/granule;assert(n<=sizeof vec);assert(mincore(p,bytes,vec)==0);size_t count=0;for(size_t i=0;i<n;i++)count+=(vec[i]&1u)!=0;return count;}
static void linux_test(void)
{
    long sys=sysconf(_SC_PAGESIZE);assert(sys>0);size_t g=(size_t)sys,cap=32*g;
    uint8_t *base=mmap(NULL,cap,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(base!=MAP_FAILED);
    memset(base,0xa5,cap);assert(resident(base,cap,g)==32);
    size_t starts[N]={0},ends[N]={0},refs[N]={0};uint32_t order[N]={0};uint64_t hashes[N]={0};
    struct exodus_page_dedup_store s={.unique={.payload=base,.payload_capacity=cap,.page_ends=ends,.page_capacity=N,.page_starts=starts,.page_order=order},.page_refs=refs,.fingerprints=hashes,.page_capacity=N};
    uint8_t a[P],b[P],out[P];memset(a,0x55,P);memset(b,0x77,P);
    assert(exodus_page_dedup_store_append_adaptive(&s,a));assert(exodus_page_dedup_store_append_adaptive(&s,a));assert(exodus_page_dedup_store_append_adaptive(&s,b));
    size_t done;assert(exodus_page_dedup_store_discard_free(&s,g,cap,linux_discard,NULL,&done));
    size_t after=resident(base,cap,g);assert(after==1&&done==31*g);
    assert(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(a,out,P));assert(exodus_page_dedup_store_restore(&s,2,out)&&!memcmp(b,out,P));
    /* Incompressible data fills a previously discarded granule on reuse. */
    uint32_t r=0x5ca7a42b;for(size_t i=0;i<P;i++){r^=r<<13;r^=r>>17;r^=r<<5;b[i]=(uint8_t)r;}
    assert(exodus_page_dedup_store_replace_adaptive(&s,1,b));assert(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(b,out,P));assert(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(a,out,P));
    assert(resident(base,cap,g)>=1);
    printf("Linux mincore: 32 -> %zu resident pages; %zu bytes discarded; shared data and subsequent write intact\n",after,done);
    assert(munmap(base,cap)==0);
}
#endif
int main(void){portable();
#if defined(__linux__)
 linux_test();
#endif
 return 0;
}
