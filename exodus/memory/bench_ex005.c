/* Deterministic EX-005 acceptance workload, shared by host and Switch.
 * Timings exclude fixture setup and final correctness checks. */
#include "page_codec.h"
#include "bdi_slab.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

enum { P=EXODUS_PAGE_BYTES, N=32768, B=7 };
static uint8_t page[P], wire[P], restored[P], small[64*20], value[4];
static uint16_t refs[64];
static struct exodus_bdi_slab slab;
static struct exodus_bdi_slab_page hot;

static void put(uint8_t *p,uint32_t v){
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));
}
static uint32_t get(const uint8_t *p){
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
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
static bool report(FILE *out,const char *name,uint64_t start,uint64_t end,uint64_t digest){
    if(end<=start||!hz())return false;
    fprintf(out,"%s,%" PRIu64 ",%u,%" PRIu64 ",%" PRIu64 ",%.3f\n",
            name,end-start,(unsigned)N,hz(),digest,
            (double)(end-start)*1e9/((double)hz()*N));
    return true;
}
bool exodus_bench_ex005(FILE *out){
    if(!out)return false;
    for(size_t w=0;w<P/4;w++)put(page+4*w,1000u+(uint32_t)(w/16)*1000u+(uint32_t)(w%16));
    size_t n=exodus_page_encode_adaptive(page,wire);
    if(n!=1289||wire[0]!=EXODUS_PAGE_BDI_VERSION)return false;
    uint64_t expected_hash=exodus_page_fingerprint(page);
    if(!exodus_page_decode(wire,n,restored)||memcmp(page,restored,P))return false;
    fprintf(out,"case,ticks,operations,tick_hz,digest,ns_per_op\n");
    for(unsigned trial=0;trial<B;trial++){
        uint64_t digest=0,oracle=0,start,end;
        for(size_t i=0;i<N;i++)oracle+=get(page+4*((i*17u)%1024u));
        start=now();
        for(size_t i=0;i<N;i++){
            uint8_t v[4];if(!exodus_page_bdi_read_word(wire,n,(i*17u)%1024u,v))return false;
            digest+=get(v);
        }
        end=now();if(digest!=oracle||!report(out,"bdi_read",start,end,digest))return false;
        digest=0;start=now();
        for(size_t i=0;i<N;i++){
            uint64_t h;if(!exodus_page_bdi_fingerprint(wire,n,&h))return false;
            digest+=h;
        }
        end=now();if(digest!=expected_hash*(uint64_t)N||!report(out,"bdi_fingerprint",start,end,digest))return false;
        digest=0;oracle=0;
        for(size_t i=0;i<N;i++)oracle+=(uint8_t)(16+i%128);
        start=now();
        for(size_t i=0;i<N;i++){
            size_t b=i%64,w=b*16+1;put(value,1000u+(uint32_t)b*1000u+16u+(uint32_t)(i%128));
            struct exodus_bdi_patch p;
            if(!exodus_page_bdi_prepare_patch(wire,n,w,value,&p)||p.new_len!=20)return false;
            digest+=p.bytes[5];
        }
        end=now();if(digest!=oracle||!report(out,"cold_patch",start,end,digest))return false;
        memset(refs,0,sizeof refs);memset(&hot,0,sizeof hot);
        slab=(struct exodus_bdi_slab){.small=small,.small_refs=refs,.small_capacity=64};
        if(!exodus_bdi_slab_import(&slab,&hot,page))return false;
        memcpy(restored,page,P);digest=0;
        start=now();
        for(size_t i=0;i<N;i++){
            size_t b=i%64,w=b*16+1;
            put(value,1000u+(uint32_t)b*1000u+16u+(uint32_t)((i/64)%128));
            if(!exodus_bdi_slab_write(&slab,&hot,w,value))return false;
        }
        end=now();
        for(size_t b=0;b<64;b++)put(restored+4*(b*16+1),1000u+(uint32_t)b*1000u+16u+((N/64-1)%128));
        uint8_t check[P];
        if(!exodus_bdi_slab_restore(&slab,&hot,check)||memcmp(restored,check,P))return false;
        digest=exodus_page_fingerprint(check);
        if(!exodus_bdi_slab_release(&slab,&hot)||!report(out,"hot_write",start,end,digest))return false;
    }
    /* Keep a rectangular CSV; metadata rows have operations=0 and no latency. */
    fprintf(out,"fixture_bytes,%zu,0,0,0,0\n",n);
    fprintf(out,"slab_payload_bytes,%zu,0,0,0,0\n",sizeof small);
    fprintf(out,"slab_ref_bytes,%zu,0,0,0,0\n",sizeof refs);
    return true;
}
#ifndef __SWITCH__
int main(void){return exodus_bench_ex005(stdout)?0:1;}
#endif
