#include "bdi_slab.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { N=4, S=256, R=256, P=4096 };
static uint8_t small[S*20],raw[R*64],oldsmall[S*20],oldraw[R*64],want[N][P],out[P];
static uint16_t sr[S],rr[R],oldsr[S],oldrr[R];
static struct exodus_bdi_slab pool={small,raw,sr,rr,S,R,NULL};
static struct exodus_bdi_slab_page pages[N];
static struct exodus_bdi_free_index index;
static uint32_t rng=0x9a8b7c6d;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void verify(void){uint16_t a[S]={0},b[R]={0};for(size_t i=0;i<N;i++)if(pages[i].active){assert(exodus_bdi_slab_restore(&pool,&pages[i],out)&&!memcmp(out,want[i],P));for(size_t k=0;k<64;k++){uint16_t id=pages[i].ids[k];if(id&32768)++b[id&32767];else ++a[id];}size_t w=rnd()%1024;uint8_t v[4];assert(exodus_bdi_slab_read(&pool,&pages[i],w,v)&&!memcmp(v,want[i]+w*4,4));}assert(!memcmp(a,sr,sizeof a)&&!memcmp(b,rr,sizeof b));}
/* Distinct IDs can share the same mask bit, including across RAW and
 * BDI classes. The filter must never accept a duplicate or reject a
 * valid colliding ID. All mutating paths retain failure atomicity. */
static void check_validation_filter(void){
 struct exodus_bdi_slab_page p={0},clone={0};
 p.active=true;
 for(size_t i=0;i<64;i++){
  size_t slot=((i/2)%4)*64+i/8;
  bool is_raw=(i&1u)!=0;
  p.ids[i]=(uint16_t)(slot|(is_raw?32768u:0u));
  (is_raw?rr:sr)[slot]=1;
 }
 memset(out,0xa5,sizeof out);
 assert(exodus_bdi_slab_restore(&pool,&p,out));
 for(size_t i=0;i<P;i++)assert(out[i]==0);
 assert(exodus_bdi_slab_clone(&pool,&clone,&p));
 assert(exodus_bdi_slab_release(&pool,&clone));
 memcpy(oldsr,sr,sizeof sr);memcpy(oldrr,rr,sizeof rr);
 uint16_t last=p.ids[63];p.ids[63]=p.ids[1];
 memset(out,0xa5,sizeof out);
 assert(!exodus_bdi_slab_restore(&pool,&p,out));
 for(size_t i=0;i<P;i++)assert(out[i]==0xa5);
 uint8_t v[4]={1};
 assert(!exodus_bdi_slab_clone(&pool,&clone,&p)&&!clone.active);
 assert(!exodus_bdi_slab_write(&pool,&p,0,v));
 assert(!exodus_bdi_slab_release(&pool,&p)&&p.active);
 assert(!memcmp(oldsr,sr,sizeof sr)&&!memcmp(oldrr,rr,sizeof rr));
 p.ids[63]=last;
 size_t slot=last&32767u;rr[slot]=0;
 assert(!exodus_bdi_slab_restore(&pool,&p,out));
 for(size_t i=0;i<P;i++)assert(out[i]==0xa5);
 rr[slot]=1;
 assert(exodus_bdi_slab_release(&pool,&p));
 for(size_t i=0;i<S;i++)assert(!sr[i]);
 for(size_t i=0;i<R;i++)assert(!rr[i]);
}
int main(void){
 check_validation_filter();
 assert(exodus_bdi_slab_attach_index(&pool,&index));
 for(size_t w=0;w<1024;w++)put(want[0]+4*w,1000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));
 assert(exodus_bdi_slab_import(&pool,&pages[0],want[0]));
 for(size_t i=1;i<N;i++){assert(exodus_bdi_slab_clone(&pool,&pages[i],&pages[0]));memcpy(want[i],want[0],P);}verify();
 size_t transitions=0,cows=0;
 for(size_t k=0;k<20000;k++){
  size_t i=rnd()%N,w=rnd()%1024,j=rnd()%N;uint8_t v[4];
  if(k%31==0&&i!=j){assert(exodus_bdi_slab_release(&pool,&pages[i]));assert(exodus_bdi_slab_clone(&pool,&pages[i],&pages[j]));memcpy(want[i],want[j],P);}
  else{
   if(k%3==0)put(v,1000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));else if(k%3==1)put(v,rnd());else put(v,1000+(uint32_t)(w/16)*1000+rnd()%128);
   struct exodus_bdi_slab_page before=pages[i];uint16_t old=before.ids[w/16];bool shared=(old&32768?rr[old&32767]:sr[old])>1;
   memcpy(oldsmall,small,sizeof small);memcpy(oldraw,raw,sizeof raw);
   assert(exodus_bdi_slab_write(&pool,&pages[i],w,v));memcpy(want[i]+4*w,v,4);uint16_t id=pages[i].ids[w/16];
   if((old^id)&32768)++transitions;
   if(shared&&old!=id)++cows;
   for(size_t b=0;b<64;b++)if(b!=w/16)assert(before.ids[b]==pages[i].ids[b]);
   for(size_t b=0;b<S;b++)if(id!=(uint16_t)b)assert(!memcmp(oldsmall+b*20,small+b*20,20));
   for(size_t b=0;b<R;b++)if(id!=(uint16_t)(32768+b))assert(!memcmp(oldraw+b*64,raw+b*64,64));
  }
  verify();assert(exodus_bdi_slab_check_index(&pool));
 }
 for(size_t i=0;i<N;i++)assert(exodus_bdi_slab_release(&pool,&pages[i]));
 verify();
 /* Exhausted raw class: both import and promotion must be atomic. */
 assert(exodus_bdi_slab_check_index(&pool));exodus_bdi_slab_detach_index(&pool);
 pool.raw_capacity=0;assert(exodus_bdi_slab_import(&pool,&pages[0],want[0])==false || exodus_bdi_slab_release(&pool,&pages[0]));
 for(size_t w=0;w<1024;w++)put(want[0]+4*w,1000+(uint32_t)(w/16)*1000+(uint32_t)(w%16));
 assert(exodus_bdi_slab_import(&pool,&pages[0],want[0]));struct exodus_bdi_slab_page saved=pages[0];uint8_t v[4];put(v,UINT32_MAX);
 memcpy(oldsmall,small,sizeof small);memcpy(oldraw,raw,sizeof raw);memcpy(oldsr,sr,sizeof sr);memcpy(oldrr,rr,sizeof rr);
 assert(!exodus_bdi_slab_write(&pool,&pages[0],0,v));assert(!memcmp(&saved,&pages[0],sizeof saved));
 memcpy(want[1],want[0],P);memcpy(want[1],v,4);assert(!exodus_bdi_slab_import(&pool,&pages[1],want[1])&&!pages[1].active);
 assert(!memcmp(oldsmall,small,sizeof small)&&!memcmp(oldraw,raw,sizeof raw)&&!memcmp(oldsr,sr,sizeof sr)&&!memcmp(oldrr,rr,sizeof rr));
 assert(exodus_bdi_slab_release(&pool,&pages[0]));verify();
 printf("20000 iterations; %zu class transitions, %zu block COWs; untouched slots, reference counts and failure atomicity passed\n",transitions,cows);
 return 0;
}
