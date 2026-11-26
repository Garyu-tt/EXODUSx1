#include "page_read.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {P=4096,N=8,S=512};
static uint8_t arena[(N+1)*P],small[S*20],raw[S*64],want[N][P],out[P];
static uint16_t sr[S],rr[S],length[N];static uint32_t start[N],order[N];
static size_t refs[N];static uint64_t fp[N];static struct exodus_hot_entry hot[N];
static struct exodus_bdi_slab slab={small,raw,sr,rr,S,S,NULL};
static struct exodus_page_dedup_store s={.unique={.payload=arena,.payload_capacity=sizeof arena,.page_capacity=N,.starts32=start,.lengths16=length,.page_order=order},.fingerprints=fp,.page_refs=refs,.page_capacity=N,.slab=&slab,.hot=hot,.hot_capacity=N};
static struct exodus_bdi_free_index free_index;
static uint32_t rng=0x63821u;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void verify(void){
 assert(exodus_page_store_validate(&s.unique));uint16_t a[S]={0},b[S]={0};size_t owners[N]={0};
 for(size_t i=0;i<s.page_count;i++){assert(refs[i]<s.unique.page_count);++owners[refs[i]];assert(exodus_page_dedup_store_restore(&s,i,out)&&!memcmp(out,want[i],P));assert(fp[refs[i]]==exodus_page_fingerprint(want[i]));size_t w=rnd()%1024;uint8_t v[4];assert(exodus_page_dedup_store_read_word(&s,i,w,v)&&!memcmp(v,want[i]+4*w,4));for(size_t j=0;j<i;j++)assert((refs[i]==refs[j])==(!memcmp(want[i],want[j],P)));}
 for(size_t i=0;i<s.unique.page_count;i++)assert(owners[i]);
 for(size_t i=0;i<N;i++)if(hot[i].page.active){assert(hot[i].physical<s.unique.page_count);assert(exodus_hot_find(&s,hot[i].physical)==&hot[i]);assert(length[hot[i].physical]==8);for(size_t k=0;k<64;k++){uint16_t id=hot[i].page.ids[k];if(id&32768){assert((id&32767)<S);++b[id&32767];}else{assert(id<S);++a[id];}}}
 assert(!memcmp(a,sr,sizeof a)&&!memcmp(b,rr,sizeof b));
}
static void append_and_fallback(void){
 for(size_t w=0;w<1024;w++)put(want[0]+4*w,1000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));
 assert(exodus_page_dedup_store_append_adaptive(&s,want[0]));
 assert(exodus_page_dedup_store_promote(&s,0));
 memcpy(want[1],want[0],P);assert(exodus_page_dedup_store_append_adaptive(&s,want[1]));assert(refs[0]==refs[1]);
 memset(want[2],0,P);assert(exodus_page_dedup_store_append_adaptive(&s,want[2]));assert(refs[0]!=refs[2]);
 /* No RAW slots: a shared BDI page must fall back to the cold tier. */
 slab.raw_capacity=0;uint8_t v[4];put(v,UINT32_MAX);
 assert(exodus_page_dedup_store_write_word(&s,1,0,v));memcpy(want[1],v,4);
 assert(exodus_hot_find(&s,refs[0])&&!exodus_hot_find(&s,refs[1]));verify();
 assert(exodus_page_dedup_store_write_word(&s,1,0,want[0]));memcpy(want[1],want[0],P);assert(refs[0]==refs[1]);verify();
 assert(exodus_page_dedup_store_demote(&s,0));verify();
 slab.raw_capacity=S;
 /* Reset after verifying every owned slab slot was released. */
 for(size_t k=0;k<S;k++)assert(!sr[k]&&!rr[k]);
 s.page_count=s.unique.page_count=s.unique.payload_used=0;
 memset(hot,0,sizeof hot);memset(arena,0,sizeof arena);memset(start,0,sizeof start);memset(length,0,sizeof length);memset(order,0,sizeof order);memset(fp,0,sizeof fp);memset(refs,0,sizeof refs);
 puts("Hot/cold append, placeholder isolation, exhausted slab fallback and remerge passed");
}
/* Exercise the public write path with a false matching fingerprint before
 * the real match. Run once with hot targets and once with cold targets. */
static void check_patched_candidates(bool hot_targets){
 uint8_t v[4];put(v,UINT32_MAX);
 for(size_t w=0;w<1024;w++)put(want[0]+4*w,1000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));
 memcpy(want[1],want[0],P);memcpy(want[1],v,4);want[1][P-1]^=1;
 memcpy(want[2],want[0],P);memcpy(want[2],v,4);
 for(size_t i=0;i<3;i++)assert(exodus_page_dedup_store_append_adaptive(&s,want[i]));
 assert(exodus_page_dedup_store_promote(&s,0));
 if(hot_targets){
  assert(exodus_page_dedup_store_promote(&s,1));
  assert(exodus_page_dedup_store_promote(&s,2));
 }
 assert(refs[0]==0&&refs[1]==1&&refs[2]==2);
 uint64_t saved=fp[1];fp[1]=fp[2]; /* inject collision, not a real hash change */
 /* Malformed candidate must fail before any replacement or slot release. */
 size_t old_count=s.unique.page_count,old_used=s.unique.payload_used;
 if(hot_targets){
  struct exodus_hot_entry *h=NULL;
  for(size_t k=0;k<N;k++)if(hot[k].page.active&&hot[k].physical==refs[1])h=&hot[k];
  assert(h);
  uint16_t id=h->page.ids[63];h->page.ids[63]=h->page.ids[62];
  assert(!exodus_page_dedup_store_write_word(&s,0,0,v));
  h->page.ids[63]=id;
 }else{
  size_t a,b;assert(exodus_page_store_extent(&s.unique,refs[1],&a,&b));
  assert(b-a>9&&arena[a]==EXODUS_PAGE_BDI_VERSION);
  arena[a+1]^=1;
  assert(!exodus_page_dedup_store_write_word(&s,0,0,v));
  arena[a+1]^=1;
 }
 assert(s.unique.page_count==old_count&&s.unique.payload_used==old_used);
 assert(refs[0]==0&&refs[1]==1&&refs[2]==2&&fp[1]==fp[2]);
 fp[1]=saved;verify();
 fp[1]=fp[2];
 assert(exodus_page_dedup_store_write_word(&s,0,0,v));
 memcpy(want[0],v,4);
 assert(s.unique.page_count==2&&refs[0]==refs[2]&&refs[1]!=refs[0]);
 fp[refs[1]]=saved;verify();
 for(size_t i=0;i<3;i++)assert(exodus_page_dedup_store_demote(&s,i));
 verify();for(size_t k=0;k<S;k++)assert(!sr[k]&&!rr[k]);
 s.page_count=s.unique.page_count=s.unique.payload_used=0;
 memset(hot,0,sizeof hot);memset(arena,0,sizeof arena);memset(start,0,sizeof start);
 memset(length,0,sizeof length);memset(order,0,sizeof order);memset(fp,0,sizeof fp);memset(refs,0,sizeof refs);
}
/* Exercise the store-level prospective hot write against a false hash
 * candidate followed by an exact match, independently hot or cold. */
static void prospective_collision_matrix(void){
 for(unsigned mode=0;mode<4;mode++){
  assert(s.page_count==0 && s.unique.page_count==0);
  for(size_t w=0;w<1024;w++)put(want[0]+4*w,1000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));
  memcpy(want[2],want[0],P);put(want[2],UINT32_MAX);
  memcpy(want[1],want[2],P);want[1][P-1]^=1;
  for(size_t i=0;i<3;i++)assert(exodus_page_dedup_store_append_adaptive(&s,want[i]));
  assert(s.unique.page_count==3 && refs[0]==0 && refs[1]==1 && refs[2]==2);
  assert(exodus_page_dedup_store_promote(&s,0));
  if(mode&1u)assert(exodus_page_dedup_store_promote(&s,1));
  if(mode&2u)assert(exodus_page_dedup_store_promote(&s,2));
  verify();
  /* Fault injection only: first candidate has the prospective hash but
   * differs at the final byte; the second candidate is genuinely equal. */
  fp[refs[1]]=fp[refs[2]];
  assert(exodus_page_dedup_store_write_word(&s,0,0,want[2]));
  memcpy(want[0],want[2],P);
  assert(s.unique.page_count==2 && refs[0]==refs[2] && refs[0]!=refs[1]);
  fp[refs[1]]=exodus_page_fingerprint(want[1]);
  verify();
  for(size_t i=0;i<3;i++)assert(exodus_page_dedup_store_demote(&s,i));
  verify();assert(exodus_bdi_slab_check_index(&slab));
  for(size_t k=0;k<S;k++)assert(!sr[k]&&!rr[k]);
  s.page_count=s.unique.page_count=s.unique.payload_used=0;
  memset(hot,0,sizeof hot);memset(arena,0,sizeof arena);memset(start,0,sizeof start);
  memset(length,0,sizeof length);memset(order,0,sizeof order);memset(fp,0,sizeof fp);memset(refs,0,sizeof refs);
 }
 puts("Four hot/cold prospective hash-collision cases passed");
}
int main(void){
 append_and_fallback();
 check_patched_candidates(true);check_patched_candidates(false);assert(exodus_bdi_slab_attach_index(&slab,&free_index));prospective_collision_matrix();rng=0x63821u;
 for(size_t i=0;i<N;i++){for(size_t w=0;w<1024;w++)put(want[i]+4*w,1000+(uint32_t)(i%3)*100000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));assert(exodus_page_dedup_store_append_adaptive(&s,want[i]));}verify();
 assert(exodus_page_dedup_store_promote(&s,0));assert(exodus_hot_find(&s,refs[3]));verify();
 uint8_t v[4];put(v,UINT32_MAX);assert(exodus_page_dedup_store_write_word(&s,3,0,v));memcpy(want[3],v,4);assert(refs[0]!=refs[3]);const struct exodus_hot_entry *a=exodus_hot_find(&s,refs[0]),*b=exodus_hot_find(&s,refs[3]);assert(a&&b&&a->page.ids[0]!=b->page.ids[0]);for(size_t k=1;k<64;k++)assert(a->page.ids[k]==b->page.ids[k]);verify();
 assert(exodus_page_dedup_store_write_word(&s,3,0,want[0]));memcpy(want[3],want[0],P);assert(refs[3]==refs[0]);verify();
 /* Demotion preserves all aliases. */
 assert(exodus_page_dedup_store_demote(&s,0));assert(!exodus_hot_find(&s,refs[0]));verify();
 size_t promotions=0,demotions=0,writes=0,merges=0;
 for(size_t k=0;k<6000;k++){size_t i=rnd()%N,j=rnd()%N,w=rnd()%1024,op=rnd()%10;
  if(op==0){assert(exodus_page_dedup_store_promote(&s,i));++promotions;}
  else if(op==1){if(!exodus_page_dedup_store_demote(&s,i)){verify();size_t m;assert(exodus_page_dedup_store_compact(&s,SIZE_MAX,&m));assert(exodus_page_dedup_store_demote(&s,i));}++demotions;}
  else if(op<4){assert(exodus_page_dedup_store_replace_adaptive(&s,i,want[j]));if(i!=j)memcpy(want[i],want[j],P);++merges;}
  else{put(v,rnd());if(!exodus_page_dedup_store_write_word(&s,i,w,v)){verify();size_t m;assert(exodus_page_dedup_store_compact(&s,SIZE_MAX,&m));assert(exodus_page_dedup_store_write_word(&s,i,w,v));}memcpy(want[i]+4*w,v,4);++writes;}
  if(k%17==0){size_t moved;assert(exodus_page_dedup_store_compact(&s,8192,&moved)&&moved<=8192);}verify();assert(exodus_bdi_slab_check_index(&slab));
 }
 for(size_t i=0;i<N;i++){assert(exodus_page_dedup_store_demote(&s,i));}verify();for(size_t i=0;i<S;i++)assert(!sr[i]&&!rr[i]);
 /* Exhausted slabs cannot promote; existing cold page remains valid. */
 assert(exodus_bdi_slab_check_index(&slab));exodus_bdi_slab_detach_index(&slab);
 slab.small_capacity=slab.raw_capacity=0;assert(!exodus_page_dedup_store_promote(&s,0));verify();
 printf("6000 hybrid operations: %zu promotions, %zu demotions, %zu writes, %zu replacements; exact bytes, aliases and slot refcounts verified\n",promotions,demotions,writes,merges);return 0;
}
