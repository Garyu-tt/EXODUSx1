#include "page_dedup_store.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum{P=EXODUS_PAGE_BYTES,N=32};
static uint8_t arena[P*(N+1)],expected[N][P],out[P];
static size_t st[N],en[N],ref[N],refbefore[N];
static uint64_t fp[N],fpbefore[N];
static uint32_t ord[N];
static struct exodus_page_dedup_store s={.unique={.payload=arena,.payload_capacity=sizeof arena,.page_ends=en,.page_capacity=N,.page_starts=st,.page_order=ord},.fingerprints=fp,.page_refs=ref,.page_capacity=N};
static uint32_t rng=18089;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void verify(void){assert(exodus_page_store_validate(&s.unique));size_t owners[N]={0};for(size_t i=0;i<N;i++){assert(ref[i]<s.unique.page_count);++owners[ref[i]];assert(exodus_page_dedup_store_restore(&s,i,out)&&!memcmp(out,expected[i],P));assert(fp[ref[i]]==exodus_page_fingerprint(expected[i]));for(size_t j=0;j<i;j++)assert((ref[i]==ref[j])==(!memcmp(expected[i],expected[j],P)));}for(size_t i=0;i<s.unique.page_count;i++)assert(owners[i]);}
int main(void){for(size_t i=0;i<N;i++){for(size_t w=0;w<P/4;w++)put(expected[i]+4*w,1000+(uint32_t)(i%4)*100000+(uint32_t)(w/16)*1000+(uint32_t)(w%16)*3);assert(exodus_page_dedup_store_append_adaptive(&s,expected[i]));}assert(s.unique.page_count==4);size_t compacts=0,bytes=0,retries=0;
for(size_t k=0;k<5000;k++){size_t i=rnd()%N,j=rnd()%N,w=rnd()%(P/4);if(k%3==0){if(!exodus_page_dedup_store_replace_adaptive(&s,i,expected[j])){verify();size_t m;assert(exodus_page_dedup_store_compact(&s,SIZE_MAX,&m));++retries;assert(exodus_page_dedup_store_replace_adaptive(&s,i,expected[j]));}if(i!=j)memcpy(expected[i],expected[j],P);}else{uint8_t v[4];put(v,rnd());if(!exodus_page_dedup_store_write_word(&s,i,w,v)){verify();size_t m;assert(exodus_page_dedup_store_compact(&s,SIZE_MAX,&m));++retries;assert(exodus_page_dedup_store_write_word(&s,i,w,v));}memcpy(expected[i]+4*w,v,4);}verify();if(k%7==0){size_t b=rnd()%8192,m;memcpy(refbefore,ref,sizeof ref);memcpy(fpbefore,fp,sizeof fp);assert(exodus_page_dedup_store_compact(&s,b,&m)&&m<=b);assert(!memcmp(ref,refbefore,sizeof ref)&&!memcmp(fp,fpbefore,sizeof fp));bytes+=m;++compacts;verify();}}
size_t m;assert(exodus_page_dedup_store_compact(&s,SIZE_MAX,&m));verify();struct exodus_page_pool_stats z;assert(exodus_page_store_pool_stats(&s.unique,&z)&&z.holes==0&&z.free_bytes==z.tail_free);printf("5000 dedup/BDI mutations, %zu budgeted compactions, %zu bytes copied, logical references and hashes preserved; %zu full-compaction retries\n",compacts,bytes,retries);}
