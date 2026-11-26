/* Integration workload: caller-owned dedup store, not an OS allocator.
 * Same deterministic trace and raw oracle for both source revisions. */
#include "page_read.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef __SWITCH__
#include <switch.h>
#endif
static uint64_t now(void){
#ifdef __SWITCH__
    return armGetSystemTick();
#else
    return (uint64_t)clock();
#endif
}
static uint64_t hz(void){
#ifdef __SWITCH__
    return armGetSystemTickFreq();
#else
    return CLOCKS_PER_SEC;
#endif
}
enum { N=32, P=4096, W=P/4, S=N*64, OPS=32768, TRIALS=7 };
static uint8_t arena[N*P], packed[S*20], raw[S*64], original[N][P], oracle[N][P], restored[P];
static uint16_t sr[S], rr[S], lengths[N];
static uint32_t starts[N], order[N];
static uint64_t fingerprints[N];
static size_t refs[N];
static struct exodus_hot_entry hot[N];
static struct exodus_bdi_free_index free_index;
static struct exodus_bdi_slab slab;
static struct exodus_page_dedup_store store;
struct operation { unsigned page, word, kind; uint8_t value[4]; };
static struct operation trace[OPS];
static uint32_t rng;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t mix(uint64_t h,uint32_t v){return (h^v)*UINT64_C(1099511628211);}
static int setup(void){
    memset(arena,0,sizeof arena);memset(packed,0,sizeof packed);memset(raw,0,sizeof raw);
    memset(sr,0,sizeof sr);memset(rr,0,sizeof rr);memset(lengths,0,sizeof lengths);
    memset(starts,0,sizeof starts);memset(order,0,sizeof order);memset(refs,0,sizeof refs);
    memset(fingerprints,0,sizeof fingerprints);memset(hot,0,sizeof hot);memset(&free_index,0,sizeof free_index);
    slab=(struct exodus_bdi_slab){packed,raw,sr,rr,S,S,NULL};
    store=(struct exodus_page_dedup_store){.unique={.payload=arena,.payload_capacity=sizeof arena,.page_capacity=N,.starts32=starts,.lengths16=lengths,.page_order=order},.fingerprints=fingerprints,.page_refs=refs,.page_capacity=N,.slab=&slab,.hot=hot,.hot_capacity=N};
    if(!exodus_bdi_slab_attach_index(&slab,&free_index))return 0;
    for(unsigned i=0;i<N;i++)if(!exodus_page_dedup_store_append_adaptive(&store,original[i]))return 0;
    if(store.unique.page_count!=8)return 0;
    for(unsigned i=0;i<4;i++)if(!exodus_page_dedup_store_promote(&store,i))return 0;
    return 1;
}
static uint64_t make_trace(void){
    rng=0x5a6c3971u;
    for(unsigned p=0;p<N;p++)for(unsigned w=0;w<W;w++){
        unsigned k=p%8,b=w/16;
        uint32_t v=(k==7||((b+3*k)%8<k))?0x40000000u+(k*W+w)*65537u:1000u+k*100000u+b*1000u+w%16;
        put(original[p]+4*w,v);
    }
    memcpy(oracle,original,sizeof oracle);
    uint64_t expected=UINT64_C(14695981039346656037);
    for(unsigned i=0;i<OPS;i++){
        struct operation *o=&trace[i];unsigned x=random32();
        o->page=x%N;o->word=random32()%W;o->kind=(x>>8)%20;
        uint8_t *p=oracle[o->page]+4*o->word;
        if(o->kind<14)expected=mix(expected,get(p));
        else if(o->kind<18){
            uint32_t value=(i%11==0)?random32():1000u+(o->page%8)*100000u+(o->word/16)*1000u+random32()%128u;
            put(o->value,value);memcpy(p,o->value,4);
        }
    }
    return expected;
}
static int verify(void){
    if(!exodus_page_store_validate(&store.unique)||!exodus_bdi_slab_check_index(&slab))return 0;
    for(unsigned p=0;p<N;p++){
        if(!exodus_page_dedup_store_restore(&store,p,restored)||memcmp(restored,oracle[p],P))return 0;
        if(refs[p]>=store.unique.page_count||fingerprints[refs[p]]!=exodus_page_fingerprint(oracle[p]))return 0;
        for(unsigned j=0;j<p;j++)if((refs[j]==refs[p])!=(memcmp(oracle[j],oracle[p],P)==0))return 0;
    }
    return 1;
}
int exodus_store_trace(FILE *out){
    if(!out||!hz())return 7;
    uint64_t expected=make_trace();
    fprintf(out,"trial,operations,ticks,tick_hz,digest,unique_pages,cold_payload,packed_slots,raw_slots,hot_reads,cold_reads,hot_writes,cold_writes,promotions,demotions\n");
    for(unsigned trial=0;trial<TRIALS;trial++){
        if(!setup())return 1;
        uint64_t digest=UINT64_C(14695981039346656037);
        unsigned hr=0,cr=0,hw=0,cw=0,up=0,down=0;
        uint64_t begin=now();
        for(unsigned i=0;i<OPS;i++){
            const struct operation *o=&trace[i];uint8_t v[4];
            bool is_hot=exodus_hot_find(&store,refs[o->page])!=NULL;
            if(o->kind<14){
                if(is_hot)++hr;else ++cr;
                if(!exodus_page_dedup_store_read_word(&store,o->page,o->word,v))return 2;
                digest=mix(digest,get(v));
            }else if(o->kind<18){
                if(is_hot)++hw;else ++cw;
                if(!exodus_page_dedup_store_write_word(&store,o->page,o->word,o->value))return 3;
            }else if(o->kind==18){
                if(!exodus_page_dedup_store_promote(&store,o->page))return 4;
                if(!is_hot)++up;
            }else{
                if(!exodus_page_dedup_store_demote(&store,o->page))return 4;
                if(is_hot)++down;
            }
        }
        uint64_t end=now();
        if(begin==UINT64_MAX||end==UINT64_MAX||end<=begin||digest!=expected||!verify())return 5;
        /* Ensure both tiers receive substantial work, not only initial migration. */
        if(hr<1024||cr<1024||hw<256||cw<256||up<128||down<128)return 6;
        size_t a=0,b=0;for(unsigned i=0;i<S;i++){a+=sr[i]!=0;b+=rr[i]!=0;}
        fprintf(out,"%u,%u,%llu,%llu,%llu,%zu,%zu,%zu,%zu,%u,%u,%u,%u,%u,%u\n",trial,(unsigned)OPS,(unsigned long long)(end-begin),(unsigned long long)hz(),(unsigned long long)digest,store.unique.page_count,store.unique.payload_used,a,b,hr,cr,hw,cw,up,down);
    }
    return ferror(out)?7:0;
}
#ifndef __SWITCH__
int main(void){return exodus_store_trace(stdout);}
#endif
