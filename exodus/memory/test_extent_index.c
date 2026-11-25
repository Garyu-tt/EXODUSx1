#include "page_store.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
enum {N=128, CAP=16384, MAX=512};
static uint8_t arena[CAP], before[CAP], want[N][MAX], occupancy[CAP];
static size_t st[N], en[N], oldst[N], olden[N], len[N];
static uint32_t ord[N], oldord[N];
static uint32_t rng=0x5ca7a42b;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static struct exodus_page_store s={.payload=arena,.payload_capacity=CAP,.page_ends=en,.page_capacity=N,.page_starts=st,.page_order=ord};
static void check(void){
 assert(exodus_page_store_validate(&s));
 memset(occupancy,0,sizeof occupancy);
 size_t used=0,tail=0;
 for(size_t i=0;i<s.page_count;i++){
  assert(en[i]-st[i]==len[i]);assert(en[i]<=CAP);assert(!memcmp(arena+st[i],want[i],len[i]));used+=len[i];if(en[i]>tail)tail=en[i];
  for(size_t j=st[i];j<en[i];j++){assert(!occupancy[j]);occupancy[j]=1;}
 }
 size_t largest=0,run=0,holes=0;for(size_t j=0;j<CAP;j++){if(!occupancy[j]){++run;if(run>largest)largest=run;}else if(run){++holes;run=0;}}
 struct exodus_page_pool_stats z;assert(exodus_page_store_pool_stats(&s,&z));assert(z.free_bytes==CAP-used&&z.largest_gap==largest&&z.tail_free==CAP-tail&&z.holes==holes&&s.payload_used==used);
}
static size_t find(size_t skip,size_t n){
 memset(occupancy,0,sizeof occupancy);for(size_t i=0;i<s.page_count;i++)if(i!=skip)memset(occupancy+st[i],1,len[i]);
 if(skip<s.page_count && st[skip]<=CAP-n){size_t j=0;while(j<n&&!occupancy[st[skip]+j])j++;if(j==n)return st[skip];}
 size_t run=0;for(size_t i=0;i<CAP;i++){run=occupancy[i]?0:run+1;if(run==n)return i+1-n;}return SIZE_MAX;
}
int main(void){
 size_t puts_ok=0,fails=0,removes=0,compacts=0,actual_moved=0;
 for(size_t step=0;step<30000;step++){
  size_t count=s.page_count, op=rnd()%10;memcpy(before,arena,CAP);memcpy(oldst,st,sizeof st);memcpy(olden,en,sizeof en);memcpy(oldord,ord,sizeof ord);
  if(op<7){
   size_t i=(op<4&&count<N)||!count?count:rnd()%count,n=1+rnd()%MAX;
   uint8_t v[MAX];for(size_t j=0;j<n;j++)v[j]=(uint8_t)rnd();
   size_t pos=find(i,n);bool ok=exodus_page_store_pool_put(&s,i,v,n);assert(ok==(pos!=SIZE_MAX));
   if(ok){assert(st[i]==pos);len[i]=n;memcpy(want[i],v,n);++puts_ok;for(size_t j=0;j<count;j++)if(j!=i)assert(st[j]==oldst[j]&&en[j]==olden[j]);}
   else {++fails;assert(!memcmp(before,arena,CAP)&&!memcmp(oldst,st,sizeof st)&&!memcmp(olden,en,sizeof en)&&!memcmp(oldord,ord,sizeof ord));}
  }else if(op<9&&count){
   size_t i=rnd()%count;assert(exodus_page_store_pool_remove(&s,i));++removes;assert(!memcmp(before,arena,CAP));for(size_t j=i+1;j<count;j++){memcpy(want[j-1],want[j],MAX);len[j-1]=len[j];assert(st[j-1]==oldst[j]&&en[j-1]==olden[j]);}
  }else{
   size_t budget=rnd()%2048,moved=SIZE_MAX,calc=0;assert(exodus_page_store_pool_compact(&s,budget,&moved));++compacts;for(size_t j=0;j<count;j++)if(st[j]!=oldst[j]){assert(st[j]<oldst[j]);calc+=len[j];}assert(moved==calc&&moved<=budget);actual_moved+=moved;
  }
  check();
  if(s.page_count>1&&step%101==0){uint32_t prev=ord[1];ord[1]=ord[0];size_t moved=123;assert(!exodus_page_store_pool_compact(&s,999,&moved)&&moved==123);ord[1]=prev;check();}
 }
 size_t moved;assert(exodus_page_store_pool_compact(&s,SIZE_MAX,&moved));check();struct exodus_page_pool_stats z;assert(exodus_page_store_pool_stats(&s,&z)&&z.holes==0&&z.tail_free==z.free_bytes);
 printf("30000 operations: puts=%zu failed=%zu removes=%zu compactions=%zu bytes_moved=%zu; oracle and rollback pass\n",puts_ok,fails,removes,compacts,actual_moved);
}
