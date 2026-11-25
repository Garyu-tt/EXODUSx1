#include "page_read.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {P=4096,N=24,CAP=N*P};
static uint8_t wide[CAP],narrow[CAP],expected[N][P],out[P];
static size_t ws[N],we[N],wr[N],nr[N];
static uint32_t wo[N],no[N],ns[N];
static uint16_t nl[N];
static uint64_t wh[N],nh[N];
static struct exodus_page_dedup_store a={.unique={.payload=wide,.payload_capacity=CAP,.page_ends=we,.page_capacity=N,.page_starts=ws,.page_order=wo},.fingerprints=wh,.page_refs=wr,.page_capacity=N};
static struct exodus_page_dedup_store b={.unique={.payload=narrow,.payload_capacity=CAP,.page_capacity=N,.page_order=no,.starts32=ns,.lengths16=nl},.fingerprints=nh,.page_refs=nr,.page_capacity=N};
static uint32_t seed=0x4ab781;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void verify(void){
 assert(exodus_page_store_validate(&a.unique)&&exodus_page_store_validate(&b.unique));
 assert(a.unique.page_count==b.unique.page_count&&a.unique.payload_used==b.unique.payload_used);
 assert(!memcmp(wide,narrow,CAP));
 for(size_t i=0;i<a.unique.page_count;i++){assert(ws[i]==ns[i]&&we[i]-ws[i]==nl[i]&&wo[i]==no[i]&&wh[i]==nh[i]);}
 for(size_t i=0;i<a.page_count;i++){assert(wr[i]==nr[i]);assert(exodus_page_dedup_store_restore(&b,i,out)&&!memcmp(out,expected[i],P));size_t w=rnd()%(P/4);uint8_t v[4];assert(exodus_page_dedup_store_read_word(&b,i,w,v)&&!memcmp(v,expected[i]+4*w,4));}
}
int main(void){
 for(size_t i=0;i<N;i++){for(size_t w=0;w<P/4;w++){uint32_t v=(uint32_t)(1000*(w/16)+w%16+i%4*100000);memcpy(expected[i]+4*w,&v,4);}assert(exodus_page_dedup_store_append_adaptive(&a,expected[i]));assert(exodus_page_dedup_store_append_adaptive(&b,expected[i]));}
 for(size_t k=0;k<4000;k++){size_t i=rnd()%N,j=rnd()%N,w=rnd()%(P/4);bool x,y;if(k%3==0){x=exodus_page_dedup_store_replace_adaptive(&a,i,expected[j]);y=exodus_page_dedup_store_replace_adaptive(&b,i,expected[j]);if(x&&i!=j)memcpy(expected[i],expected[j],P);}else{uint32_t v=rnd();x=exodus_page_dedup_store_write_word(&a,i,w,(uint8_t*)&v);y=exodus_page_dedup_store_write_word(&b,i,w,(uint8_t*)&v);if(x)memcpy(expected[i]+4*w,&v,4);}assert(x==y);if(k%7==0){size_t budget=rnd()%8192,m,n;assert(exodus_page_dedup_store_compact(&a,budget,&m));assert(exodus_page_dedup_store_compact(&b,budget,&n));assert(m==n&&m<=budget);}verify();}
 uint16_t old=nl[0];nl[0]=0;assert(!exodus_page_store_validate(&b.unique));nl[0]=old;
 uint32_t start=ns[0];ns[0]=UINT32_MAX;assert(!exodus_page_store_validate(&b.unique));ns[0]=start;
 struct exodus_page_store bad=b.unique;bad.page_ends=we;assert(!exodus_page_store_validate(&bad));
 printf("4000 differential mutations: identical wide/compact payload, offsets, hashes, direct reads and compaction; %zu -> %zu bytes per extent+order\n",2*sizeof(size_t)+sizeof(uint32_t),sizeof(uint32_t)+sizeof(uint16_t)+sizeof(uint32_t));
 return 0;
}
