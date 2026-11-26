/* QA-only bounded cold-store replay of real CPU-readable file contents.
 * No global allocator hooks, no GPU mapping, no production file mutations. */
#include "page_read.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
enum { N=32, P=EXODUS_PAGE_BYTES };
static uint8_t input[N][P],payload[N*P],restored[P];
static size_t ends[N],refs[N];
static uint64_t fingerprints[N];
struct totals { uint64_t bytes,groups,pages,unique,payload,metadata,tail; };
static int flush_group(size_t count,struct totals *t){
    struct exodus_page_dedup_store s={
        .unique={.payload=payload,.payload_capacity=sizeof payload,.page_ends=ends,.page_capacity=N},
        .fingerprints=fingerprints,.page_refs=refs,.page_capacity=N
    };
    memset(ends,0,sizeof ends);memset(refs,0,sizeof refs);memset(fingerprints,0,sizeof fingerprints);
    for(size_t p=0;p<count;p++)if(!exodus_page_dedup_store_append_adaptive(&s,input[p]))return 0;
    for(size_t p=0;p<count;p++){
        if(!exodus_page_dedup_store_restore(&s,p,restored)||memcmp(restored,input[p],P))return 0;
        for(size_t w=0;w<P/4;w++){
            uint8_t out[4];
            if(!exodus_page_dedup_store_read_word(&s,p,w,out)||memcmp(out,input[p]+4*w,4))return 0;
        }
        for(size_t j=0;j<p;j++)if((refs[p]==refs[j])!=(memcmp(input[p],input[j],P)==0))return 0;
    }
    ++t->groups;t->pages+=count;t->unique+=s.unique.page_count;t->payload+=s.unique.payload_used;
    t->metadata+=s.unique.page_count*(sizeof(size_t)+sizeof(uint64_t))+count*sizeof(size_t)+sizeof(s);
    return 1;
}
int main(int argc,char **argv){
    if(argc!=2)return 1;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    struct totals t={0};size_t count=0;int ok=1;
    for(;;){
        size_t n=fread(input[count],1,P,f);
        if(n<P){
            if(ferror(f)){ok=0;break;}
            t.tail=n;t.bytes+=n;
            if(count&&!flush_group(count,&t))ok=0;
            break;
        }
        t.bytes+=P;
        if(++count==N){if(!flush_group(count,&t)){ok=0;break;}count=0;}
    }
    if(fclose(f)!=0)ok=0;
    if(!ok)return 3;
    printf("{\"input_bytes\":%" PRIu64 ",\"groups\":%" PRIu64 ",\"pages\":%" PRIu64 ",\"unique\":%" PRIu64 ",\"payload_bytes\":%" PRIu64 ",\"active_metadata_bytes\":%" PRIu64 ",\"tail_bytes\":%" PRIu64 ",\"active_total_bytes\":%" PRIu64 ",\"reserved_backing_per_group\":%zu}\n",
        t.bytes,t.groups,t.pages,t.unique,t.payload,t.metadata,t.tail,t.payload+t.metadata+t.tail,
        sizeof(payload)+sizeof(ends)+sizeof(refs)+sizeof(fingerprints)+sizeof(struct exodus_page_dedup_store));
    return ferror(stdout)?4:0;
}
