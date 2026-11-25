#include "page_dedup_store.h"
#include "page_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define C(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
enum { P=EXODUS_PAGE_BYTES, L=8 };
static uint8_t backing[L*P], expected[L][P], page[P];
static size_t ends[L], refs[L];
static uint64_t hashes[L];
static struct exodus_page_dedup_store s={
 .unique={.payload=backing,.payload_capacity=sizeof(backing),.page_ends=ends,.page_capacity=L},
 .fingerprints=hashes,.page_refs=refs,.page_capacity=L};
static unsigned rng=12345;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void verify(void){
 size_t owners[L]={0};
 for(size_t i=0;i<s.page_count;i++){
  C(exodus_page_dedup_store_restore(&s,i,page));
  C(!memcmp(page,expected[i],P));
  C(refs[i]<s.unique.page_count);++owners[refs[i]];
  size_t w=next()%(P/4);uint8_t v[4];
  C(exodus_page_dedup_store_read_word(&s,i,w,v));
  C(!memcmp(v,expected[i]+w*4,4));
 }
 for(size_t i=0;i<s.unique.page_count;i++) C(owners[i]>0);
 for(size_t i=0;i<s.page_count;i++)
  for(size_t j=0;j<i;j++)
   C((refs[i]==refs[j])==(!memcmp(expected[i],expected[j],P)));
}
/* Non-BDI fallback: shared zero, repeated-word and tile pages, and
 * RAW/sparse pages. No-op must preserve the entire physical state;
 * changed word must split then merge with the untouched alias. */
static void check_fallback_writes(void){
 uint8_t storage[3*P],original[P],changed[P],restored[P],saved[3*P];
 size_t e[3],r[3],se[3],sr[3];uint64_t h[3],sh[3];
 for(unsigned kind=0;kind<5;kind++){
  memset(storage,0xa5,sizeof storage);memset(e,0,sizeof e);
  memset(r,0,sizeof r);memset(h,0,sizeof h);
  for(size_t i=0;i<P;i++)
   original[i]=(uint8_t)(kind==0?0:kind==1?0x71:kind==2?1+i%8:
       kind==3?(i<64?1+i:0):next());
  struct exodus_page_dedup_store t={
   .unique={.payload=storage,.payload_capacity=sizeof storage,.page_ends=e,.page_capacity=3},
   .fingerprints=h,.page_refs=r,.page_capacity=3};
  C(exodus_page_dedup_store_append_adaptive(&t,original));
  C(exodus_page_dedup_store_append_adaptive(&t,original));
  C(t.unique.page_count==1);
  size_t used=t.unique.payload_used;
  C(used==P||!(used&1u)||storage[0]!=EXODUS_PAGE_BDI_VERSION);
  for(size_t w=0;w<P/4;w+=31){
   memcpy(saved,storage,sizeof saved);memcpy(se,e,sizeof e);
   memcpy(sr,r,sizeof r);memcpy(sh,h,sizeof h);
   C(exodus_page_dedup_store_write_word(&t,0,w,original+4*w));
   C(t.unique.page_count==1&&t.unique.payload_used==used);
   C(!memcmp(saved,storage,sizeof saved)&&!memcmp(se,e,sizeof e));
   C(!memcmp(sr,r,sizeof r)&&!memcmp(sh,h,sizeof h));
   memcpy(changed,original,P);changed[4*w]^=0x80;
   C(exodus_page_dedup_store_write_word(&t,0,w,changed+4*w));
   C(t.unique.page_count==2&&r[0]!=r[1]);
   C(h[r[0]]==exodus_page_fingerprint(changed));
   C(exodus_page_dedup_store_restore(&t,0,restored)&&!memcmp(restored,changed,P));
   C(exodus_page_dedup_store_restore(&t,1,restored)&&!memcmp(restored,original,P));
   C(exodus_page_dedup_store_write_word(&t,0,w,original+4*w));
   C(t.unique.page_count==1&&r[0]==r[1]&&t.unique.payload_used==used);
  }
  /* Malformed logical reference must reject even an unchanged write. */
  r[1]=t.unique.page_count;memcpy(saved,storage,sizeof saved);
  memcpy(se,e,sizeof e);memcpy(sr,r,sizeof r);memcpy(sh,h,sizeof h);
  C(!exodus_page_dedup_store_write_word(&t,0,0,original));
  C(!memcmp(saved,storage,sizeof saved)&&!memcmp(se,e,sizeof e));
  C(!memcmp(sr,r,sizeof r)&&!memcmp(sh,h,sizeof h));
 }
}
int main(void){
 check_fallback_writes();
 for(size_t i=0;i<L;i++){memset(expected[i],(int)(i%3),P);C(exodus_page_dedup_store_append_adaptive(&s,expected[i]));}
 C(s.unique.page_count==3); verify();
 for(size_t i=0;i<2000;i++){
  size_t a=next()%L,b=next()%L,w=next()%(P/4);
  if(i%3==0){C(exodus_page_dedup_store_replace_adaptive(&s,a,expected[b]));if(a!=b)memcpy(expected[a],expected[b],P);}
  else {uint8_t v[4];for(size_t k=0;k<4;k++)v[k]=(uint8_t)next();
    C(exodus_page_dedup_store_write_word(&s,a,w,v));memcpy(expected[a]+4*w,v,4);}
  verify();
 }
 /* Force capacity failure on a shared page, with complete state comparison. */
 for(size_t i=1;i<L;i++){C(exodus_page_dedup_store_replace_adaptive(&s,i,expected[0]));memcpy(expected[i],expected[0],P);}
 C(s.unique.page_count==1);verify();
 uint8_t before[sizeof(backing)];size_t e[L],r[L];uint64_t h[L];
 memcpy(before,backing,sizeof(before));memcpy(e,ends,sizeof(e));memcpy(r,refs,sizeof(r));memcpy(h,hashes,sizeof(h));
 s.unique.payload_capacity=s.unique.payload_used;
 size_t used=s.unique.payload_used;uint8_t v[4]={1,2,3,4};v[0]=expected[0][0]^1;
 C(!exodus_page_dedup_store_write_word(&s,0,0,v));
 C(s.unique.page_count==1&&s.page_count==L&&s.unique.payload_used==used);
 C(!memcmp(before,backing,sizeof(before))&&!memcmp(e,ends,sizeof(e))&&!memcmp(r,refs,sizeof(r))&&!memcmp(h,hashes,sizeof(h)));
 C(!exodus_page_dedup_store_write_word(&s,L,0,v));
 C(!exodus_page_dedup_store_write_word(&s,0,P/4,v));
 verify();puts("COW: 2000 mutations, logical aliases, reclamation and failure atomicity passed");
 return 0;
}
