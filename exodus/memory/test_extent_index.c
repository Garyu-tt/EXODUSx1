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
/* Exercise stable-start updates in both index layouts; preserve full
 * validation and the first-fit policy when a relocation is required. */
static void check_stable_extent(bool compact){
 uint8_t bytes[64]={0},v[16],saved[64];
 memset(v,0x53,sizeof v);
 size_t starts[4]={0},ends[4]={0};
 uint32_t cs[4]={0},order[4]={0},oldorder[4];
 uint16_t cl[4]={0};
 struct exodus_page_store p={.payload=bytes,.payload_capacity=sizeof bytes,
  .page_capacity=4,.page_order=order};
 if(compact){p.starts32=cs;p.lengths16=cl;}
 else{p.page_starts=starts;p.page_ends=ends;}
 for(size_t i=0;i<3;i++)assert(exodus_page_store_pool_put(&p,i,v,8));
 memcpy(oldorder,order,sizeof order);
 assert(exodus_page_store_pool_can_put(&p,1,8));
 assert(exodus_page_store_pool_put(&p,1,v,8));
 assert(!memcmp(order,oldorder,sizeof order));
 assert(exodus_page_store_pool_put(&p,1,v,4));
 size_t a,b;assert(exodus_page_store_extent(&p,1,&a,&b)&&a==8&&b==12);
 assert(!memcmp(order,oldorder,sizeof order)&&p.payload_used==20);
 assert(exodus_page_store_pool_put(&p,1,v,6));
 assert(exodus_page_store_extent(&p,1,&a,&b)&&a==8&&b==14);
 assert(!memcmp(order,oldorder,sizeof order)&&p.payload_used==22);
 /* An append reuses the two-byte interior gap. */
 assert(exodus_page_store_pool_put(&p,3,v,2));
 assert(exodus_page_store_extent(&p,3,&a,&b)&&a==14&&b==16);
 assert(order[0]==0&&order[1]==1&&order[2]==3&&order[3]==2);
 /* Growth cannot fit there: relocate by first fit after the tail. */
 assert(exodus_page_store_pool_put(&p,1,v,12));
 assert(exodus_page_store_extent(&p,1,&a,&b)&&a==24&&b==36);
 assert(order[0]==0&&order[1]==3&&order[2]==2&&order[3]==1);
 assert(exodus_page_store_validate(&p)&&p.payload_used==30);
 /* Even a shrinking replacement must reject a corrupt ordered index. */
 order[1]=order[0];
 memcpy(saved,bytes,sizeof bytes);memcpy(oldorder,order,sizeof order);
 assert(!exodus_page_store_pool_can_put(&p,1,1));
 assert(!exodus_page_store_pool_put(&p,1,v,1));
 assert(!memcmp(saved,bytes,sizeof bytes)&&!memcmp(oldorder,order,sizeof order));
 assert(p.payload_used==30&&p.page_count==4);
 order[1]=3;assert(exodus_page_store_validate(&p));
 /* Physical indices now differ from address order. Exercise removal,
  * tail growth, relocation to the head, and insertion into the middle. */
 assert(exodus_page_store_pool_remove(&p,0));
 assert(order[0]==2&&order[1]==1&&order[2]==0);
 assert(exodus_page_store_pool_put(&p,0,v,16));
 assert(exodus_page_store_extent(&p,0,&a,&b)&&a==24&&b==40);
 assert(exodus_page_store_pool_put(&p,2,v,8));
 assert(exodus_page_store_extent(&p,2,&a,&b)&&a==0&&b==8);
 assert(exodus_page_store_pool_put(&p,3,v,8));
 assert(exodus_page_store_extent(&p,3,&a,&b)&&a==8&&b==16);
 assert(order[0]==2&&order[1]==3&&order[2]==1&&order[3]==0);
 assert(exodus_page_store_pool_remove(&p,1));
 assert(order[0]==1&&order[1]==2&&order[2]==0);
 assert(exodus_page_store_validate(&p)&&p.payload_used==32);
}
int main(void){
 check_stable_extent(false);check_stable_extent(true);
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
