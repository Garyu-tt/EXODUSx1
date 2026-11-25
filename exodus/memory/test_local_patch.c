#include "page_read.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {P=4096,N=4};
static uint8_t arena[N*P],original[P],expected[P],out[P],before[N*P];
static size_t ends[N],refs[N];static uint64_t hashes[N];static uint32_t starts[N],order[N];static uint16_t lengths[N];
static uint32_t rng=0x65a11u;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void run(int pool){
 memset(arena,0,sizeof arena);memset(ends,0,sizeof ends);memset(refs,0,sizeof refs);memset(hashes,0,sizeof hashes);memset(starts,0,sizeof starts);memset(order,0,sizeof order);memset(lengths,0,sizeof lengths);
 struct exodus_page_dedup_store s={.unique={.payload=arena,.payload_capacity=sizeof arena,.page_capacity=N,.page_ends=pool?NULL:ends,.page_order=pool?order:NULL,.starts32=pool?starts:NULL,.lengths16=pool?lengths:NULL},.fingerprints=hashes,.page_refs=refs,.page_capacity=N};
 for(size_t i=0;i<P/4;i++)put(original+4*i,10000+(uint32_t)(i/16)*1000+(uint32_t)(i%16)*3);
 memcpy(expected,original,P);assert(exodus_page_dedup_store_append_adaptive(&s,original));
 uint8_t other[P];memset(other,0x55,P);assert(exodus_page_dedup_store_append_adaptive(&s,other));
 size_t a,b;assert(exodus_page_store_extent(&s.unique,refs[0],&a,&b)&&b-a==1289);
 for(size_t k=0;k<10000;k++){
  size_t word=rnd()%(P/4);uint8_t value[4];put(value,10000+(uint32_t)(word/16)*1000+rnd()%128);
  struct exodus_bdi_patch p;assert(exodus_page_bdi_prepare_patch(arena+a,b-a,word,value,&p)&&p.old_len==20&&p.new_len==20);
  memcpy(before,arena,sizeof arena);assert(exodus_page_dedup_store_write_word(&s,0,word,value));memcpy(expected+4*word,value,4);
  assert(!memcmp(before,arena,a+p.offset));assert(!memcmp(before+a+p.offset+20,arena+a+p.offset+20,sizeof arena-a-p.offset-20));
  size_t x,y;assert(exodus_page_store_extent(&s.unique,refs[0],&x,&y)&&x==a&&y==b);assert(hashes[refs[0]]==exodus_page_fingerprint(expected));
  assert(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(out,expected,P));assert(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(out,other,P));
 }
 /* Shared backing still splits, then merges without copying an edited page. */
 assert(exodus_page_dedup_store_append_adaptive(&s,expected));assert(refs[0]==refs[2]);uint8_t v[4];put(v,10001);assert(exodus_page_dedup_store_write_word(&s,2,0,v));assert(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(out,expected,P));assert(exodus_page_dedup_store_write_word(&s,2,0,expected));assert(refs[0]==refs[2]);
 /* Size-changing mutation still works; RAW block then local RAW update. */
 put(v,0xffffffffu);assert(exodus_page_dedup_store_write_word(&s,0,0,v));memcpy(expected,v,4);assert(refs[0]!=refs[2]);assert(exodus_page_store_extent(&s.unique,refs[0],&a,&b)&&b-a==1333);put(v,0x12345678u);memcpy(before,arena,sizeof arena);assert(exodus_page_dedup_store_write_word(&s,0,0,v));memcpy(expected,v,4);assert(!memcmp(before,arena,a+9)&&!memcmp(before+a+73,arena+a+73,sizeof arena-a-73));assert(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(out,expected,P));
 struct exodus_bdi_patch bad,saved;memset(&bad,0xa5,sizeof bad);memcpy(&saved,&bad,sizeof bad);assert(!exodus_page_bdi_prepare_patch(arena+a,b-a-1,0,v,&bad)&&!memcmp(&bad,&saved,sizeof bad));
 printf("%s: 10000 local BDI writes, RAW local update, COW and merge passed\n",pool?"compact pool":"packed");
}
int main(void){run(0);run(1);return 0;}
