#include "page_read.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {P=4096,N=6,S=384};
static uint8_t arena[(N+1)*P],small[S*20],raw[S*64],want[N][P],out[P],scores[N];
static uint16_t sr[S],rr[S],length[N],scratch[N];static uint32_t start[N],order[N];
static size_t refs[N];static uint64_t fp[N];static struct exodus_hot_entry hot[N];
static struct exodus_bdi_slab slab={small,raw,sr,rr,S,S};
static struct exodus_page_heat heat={scores,scratch,0};
static struct exodus_page_dedup_store s={.unique={.payload=arena,.payload_capacity=sizeof arena,.page_capacity=N,.starts32=start,.lengths16=length,.page_order=order},.fingerprints=fp,.page_refs=refs,.page_capacity=N,.slab=&slab,.hot=hot,.hot_capacity=N,.heat=&heat};
static uint32_t rng=0x65a11u;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static bool ishot(size_t i){return exodus_hot_find(&s,refs[i])!=NULL;}
static struct exodus_heat_result tick(size_t budget){struct exodus_heat_result r;assert(exodus_page_dedup_store_heat_tick(&s,8,2,512,budget,&r));assert(r.attempted<=budget&&r.attempted==r.promoted+r.demoted+r.failed);return r;}
static void verify(void){assert(exodus_page_store_validate(&s.unique));uint16_t a[S]={0},b[S]={0};for(size_t i=0;i<N;i++){assert(exodus_page_dedup_store_restore(&s,i,out)&&!memcmp(out,want[i],P));assert(fp[refs[i]]==exodus_page_fingerprint(want[i]));for(size_t j=0;j<i;j++)assert((refs[i]==refs[j])==(!memcmp(want[i],want[j],P)));}for(size_t i=0;i<N;i++)if(hot[i].page.active){for(size_t j=0;j<64;j++){uint16_t id=hot[i].page.ids[j];assert((id&32767)<S);if(id&32768)++b[id&32767];else ++a[id];}}assert(!memcmp(a,sr,sizeof a)&&!memcmp(b,rr,sizeof b));}
int main(void){
 for(size_t i=0;i<N;i++){if(i<4)for(size_t w=0;w<1024;w++)put(want[i]+4*w,1000+(uint32_t)(i/2)*100000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));assert(exodus_page_dedup_store_append_adaptive(&s,want[i]));}
 for(size_t i=0;i<4;i++){assert(exodus_page_dedup_store_write_word(&s,0,0,want[0]));assert(exodus_page_dedup_store_write_word(&s,1,0,want[1]));}
 assert(tick(6).promoted==0); /* aliases use max, not sum */
 for(size_t i=0;i<6;i++)assert(exodus_page_dedup_store_write_word(&s,0,0,want[0]));
 assert(scores[0]==8&&tick(6).promoted==1&&ishot(0)&&ishot(1));
 assert(tick(6).demoted==0);assert(tick(6).demoted==1&&!ishot(0));
 for(size_t i=0;i<300;i++)assert(exodus_page_dedup_store_write_word(&s,4,0,want[4]));
 assert(scores[4]==255&&tick(6).promoted==0&&!ishot(4)); /* cheap zero page */
 uint8_t score=scores[0];assert(!exodus_page_dedup_store_write_word(&s,0,1024,want[0])&&scores[0]==score);
 assert(exodus_page_dedup_store_replace_adaptive(&s,0,want[0])&&scores[0]==score+1);
 uint8_t v[4];put(v,UINT32_MAX);score=scores[0];assert(exodus_page_dedup_store_write_word(&s,0,0,v));memcpy(want[0],v,4);assert(scores[0]==score+1); /* fallback counts once */
 struct exodus_heat_result r={.attempted=123};assert(!exodus_page_dedup_store_heat_tick(&s,2,2,512,1,&r)&&r.attempted==123);
 for(size_t i=0;i<10;i++)assert(exodus_page_dedup_store_write_word(&s,2,0,want[2]));
 slab.small_capacity=0;assert(tick(1).failed==1&&!ishot(2));slab.small_capacity=S;verify();
 size_t promotions=0,demotions=0,failures=0;
 for(size_t k=0;k<5000;k++){
  size_t i=(k/256)%4,w=rnd()%1024,j=rnd()%N;
  if(k%11==0){assert(exodus_page_dedup_store_replace_adaptive(&s,i,want[j]));if(i!=j)memcpy(want[i],want[j],P);}
  else{put(v,rnd());if(!exodus_page_dedup_store_write_word(&s,i,w,v)){size_t moved;assert(exodus_page_dedup_store_compact(&s,SIZE_MAX,&moved));assert(exodus_page_dedup_store_write_word(&s,i,w,v));}memcpy(want[i]+4*w,v,4);}
  if(k%16==15){r=tick(2);promotions+=r.promoted;demotions+=r.demoted;failures+=r.failed;}
  verify();
 }
 assert(promotions&&demotions);printf("5000 policy mutations, %zu promotions, %zu demotions, %zu failed attempts; exact bytes, aliases and slot counts verified\n",promotions,demotions,failures);
 return 0;
}
