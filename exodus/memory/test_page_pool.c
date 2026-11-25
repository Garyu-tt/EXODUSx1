#include "page_read.h"
#include "page_compact_index.h"
#include "page_integrity_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define C(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
enum{P=EXODUS_PAGE_BYTES,L=8};
static uint8_t backing[L*P],expected[L][P],out[P],raw[P];
static size_t starts[L],ends[L],refs[L];
static uint64_t hashes[L];
static uint32_t order[L];
static struct exodus_page_dedup_store s={
 .unique={.payload=backing,.payload_capacity=sizeof(backing),.page_ends=ends,.page_capacity=L,.page_starts=starts,.page_order=order},
 .fingerprints=hashes,.page_refs=refs,.page_capacity=L};
static unsigned rng=18277;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void put(uint8_t*p,uint32_t v){for(unsigned j=0;j<4;j++)p[j]=(uint8_t)(v>>(j*8));}
static void verify(void){
 C(exodus_page_store_validate(&s.unique));
 size_t own[L]={0};
 for(size_t i=0;i<s.page_count;i++){
  C(exodus_page_dedup_store_restore(&s,i,out)&&!memcmp(out,expected[i],P));
  C(refs[i]<s.unique.page_count);++own[refs[i]];
  C(hashes[refs[i]]==exodus_page_fingerprint(expected[i]));
  size_t w=next()%(P/4);uint8_t v[4];
  C(exodus_page_dedup_store_read_word(&s,i,w,v)&&!memcmp(v,expected[i]+4*w,4));
  for(size_t j=0;j<i;j++)C((refs[i]==refs[j])==(!memcmp(expected[i],expected[j],P)));
 }
 for(size_t i=0;i<s.unique.page_count;i++)C(own[i]>0);
}
int main(void){
 /* Grow a BDI page while its neighbour stays byte-for-byte in place. */
 for(size_t i=0;i<P/4;i++)put(expected[0]+4*i,10000+(uint32_t)(i/16)*1000+(uint32_t)(i%16)*3);
 memset(expected[1],0x72,P);
 for(size_t i=0;i<2;i++)C(exodus_page_dedup_store_append_adaptive(&s,expected[i]));
 size_t neighbor=starts[refs[1]],n=ends[refs[1]]-neighbor;
 uint8_t saved[P];memcpy(saved,backing+neighbor,n);
 uint8_t v[4]={0xff,0xff,0xff,0xff};
 C(exodus_page_dedup_store_write_word(&s,0,0,v));memcpy(expected[0],v,4);
 C(starts[refs[1]]==neighbor&&!memcmp(saved,backing+neighbor,n));
 C(starts[refs[0]]!=0);
 /* Return to the shared page: old extent becomes reusable. */
 C(exodus_page_dedup_store_replace_adaptive(&s,0,expected[1]));memcpy(expected[0],expected[1],P);
 C(s.unique.page_count==1&&starts[0]==neighbor);
 memset(expected[2],0x14,P);C(exodus_page_dedup_store_append_adaptive(&s,expected[2]));
 C(starts[refs[2]]==0);
 for(size_t i=3;i<L;i++){memcpy(expected[i],expected[2],P);C(exodus_page_dedup_store_append_adaptive(&s,expected[i]));}
 verify();
 for(size_t i=0;i<2000;i++){
  size_t a=next()%L,b=next()%L,w=next()%(P/4);
  if(i%3==0){C(exodus_page_dedup_store_replace_adaptive(&s,a,expected[b]));if(a!=b)memcpy(expected[a],expected[b],P);}
  else{put(v,next());C(exodus_page_dedup_store_write_word(&s,a,w,v));memcpy(expected[a]+4*w,v,4);}
  verify();
  if(i%17==0){size_t moved;C(exodus_page_dedup_store_compact(&s,500,&moved)&&moved<=500);verify();}
 }
 /* Independent base pool, full capacity and fragmented allocation rollback. */
 uint8_t buf[160]={0},before[160],enc[72];size_t st[3]={0},en[3]={0};uint32_t ord[3]={0};
 struct exodus_page_store p={.payload=buf,.payload_capacity=sizeof(buf),.page_starts=st,.page_ends=en,.page_capacity=3,.page_order=ord};
 memset(enc,0,72);enc[0]=1;
 C(exodus_page_store_pool_put(&p,0,enc,72));
 C(exodus_page_store_pool_put(&p,1,enc,72));
 C(exodus_page_store_pool_remove(&p,0));
 uint8_t zero[8]={0};
 C(exodus_page_store_pool_put(&p,1,zero,8)); /* use hole at zero */
 C(st[0]==72&&st[1]==0);
 size_t bst[3],ben[3];memcpy(bst,st,sizeof(st));memcpy(ben,en,sizeof(en));
 memcpy(before,buf,sizeof(buf));
 C(!exodus_page_store_pool_put(&p,2,enc,72));
 C(!memcmp(before,buf,sizeof(buf))&&p.page_count==2&&p.payload_used==80);
 C(!memcmp(st,bst,sizeof(st))&&!memcmp(en,ben,sizeof(en)));
 st[1]=80;en[1]=88;C(!exodus_page_store_validate(&p));st[1]=0;en[1]=8;

 struct exodus_page_pool_stats stats;
 C(exodus_page_store_pool_stats(&p,&stats));
 C(stats.free_bytes==80&&stats.largest_gap==64&&stats.tail_free==16&&stats.holes==1);
 size_t moved=999;
 C(exodus_page_store_pool_compact(&p,71,&moved)&&moved==0);
 C(!memcmp(before,buf,sizeof(buf)));
 C(exodus_page_store_pool_compact(&p,72,&moved)&&moved==72);
 C(st[0]==8&&st[1]==0&&en[0]==80);
 C(exodus_page_store_pool_stats(&p,&stats)&&stats.largest_gap==80&&stats.tail_free==80&&stats.holes==0);
 C(exodus_page_store_pool_put(&p,2,enc,72));
 C(exodus_page_store_validate(&p));
 uint32_t saved_order=ord[1];ord[1]=ord[0];moved=999;
 memcpy(before,buf,sizeof(buf));
 C(!exodus_page_store_pool_compact(&p,160,&moved)&&moved==999&&!memcmp(before,buf,sizeof(buf)));
 ord[1]=saved_order;
 C(exodus_page_store_validate(&p));
 struct exodus_page_compact_index idx;uint16_t lengths[3];size_t points[3];
 C(!exodus_page_compact_build(&p,&idx,lengths,3,points,3));
 /* Integrity wrapper operates over the very same extent layout. */
 uint8_t ibuf[2*P];size_t ist[2],ien[2];uint32_t crc[2];
 struct exodus_page_integrity_store integrity={.pages={.payload=ibuf,.payload_capacity=sizeof(ibuf),.page_starts=ist,.page_ends=ien,.page_capacity=2},.checksums=crc,.checksum_capacity=2};
 memset(raw,0x55,P);C(exodus_page_integrity_store_append_adaptive(&integrity,raw));
 memset(raw,0x77,P);C(exodus_page_integrity_store_replace_adaptive(&integrity,0,raw));
 C(exodus_page_integrity_store_restore(&integrity,0,out)&&!memcmp(raw,out,P));
 size_t copied;C(exodus_page_dedup_store_compact(&s,SIZE_MAX,&copied));verify();
 C(exodus_page_store_pool_stats(&s.unique,&stats)&&stats.tail_free==stats.free_bytes&&stats.holes==0);
 puts("Indexed pool: linear validation, budgeted compaction and fragmentation recovery passed");
 puts("Pool: 2000 mutations, unchanged neighbour, hole reuse, fragmented rollback, integrity passed");
 return 0;
}
