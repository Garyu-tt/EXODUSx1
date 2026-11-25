#include "bdi_slab.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {S=32768,N=S/64,P=4096};
static uint8_t small[S*20],raw[64*64],page[P],out[P];
static uint16_t sr[S],rr[64];
static struct exodus_bdi_slab_page pages[N],extra;
static struct exodus_bdi_free_index idx;
/* Exercise both size classes independently and prove rollback when the
 * second class runs out after the first class has reserved slots. */
static void mixed_class_rollback(void)
{
 static uint8_t sm[64*20], rw[129*64], mixed[P], dense[P];
 static uint16_t sm_refs[64], rw_refs[129];
 static struct exodus_bdi_free_index ix, before;
 struct exodus_bdi_slab s={sm,rw,sm_refs,rw_refs,64,129,NULL};
 struct exodus_bdi_slab_page a={0}, b={0}, m={0};
 memset(sm,0xa5,sizeof sm);
 memset(rw,0x5a,sizeof rw);
 /* Every dense block has a 32-bit range of 256, forcing RAW. */
 for(size_t i=0;i<64;i++){
  dense[i*64+5]=1;
  if(i%2) mixed[i*64+5]=1;
 }
 assert(exodus_bdi_slab_attach_index(&s,&ix));
 assert(exodus_bdi_slab_import(&s,&a,dense));
 assert(exodus_bdi_slab_import(&s,&b,dense));
 assert(ix.cls[0].bits[0]==UINT64_MAX);
 assert(ix.cls[1].bits[0]==0&&ix.cls[1].bits[1]==0&&ix.cls[1].bits[2]==1);
 memcpy(&before,&ix,sizeof ix);
 assert(!exodus_bdi_slab_import(&s,&m,mixed)&&!m.active);
 assert(!memcmp(&before,&ix,sizeof ix));
 for(size_t i=0;i<64;i++)assert(sm_refs[i]==0);
 for(size_t i=0;i<sizeof sm;i++)assert(sm[i]==0xa5);
 for(size_t i=0;i<128;i++)assert(rw_refs[i]==1);
 assert(rw_refs[128]==0);
 for(size_t i=128*64;i<sizeof rw;i++)assert(rw[i]==0x5a);
 assert(exodus_bdi_slab_restore(&s,&a,out)&&!memcmp(dense,out,P));
 assert(exodus_bdi_slab_restore(&s,&b,out)&&!memcmp(dense,out,P));
 assert(exodus_bdi_slab_check_index(&s));
 assert(exodus_bdi_slab_release(&s,&b));
 assert(exodus_bdi_slab_import(&s,&m,mixed));
 for(size_t i=0;i<64;i++){
  uint16_t expected=(uint16_t)(i%2?(32768+64+i/2):i/2);
  assert(m.ids[i]==expected);
 }
 assert(exodus_bdi_slab_restore(&s,&m,out)&&!memcmp(mixed,out,P));
 assert(exodus_bdi_slab_check_index(&s));
 assert(exodus_bdi_slab_release(&s,&m));
 assert(exodus_bdi_slab_release(&s,&a));
 assert(exodus_bdi_slab_check_index(&s));
 assert(ix.cls[0].bits[0]==UINT64_MAX);
 assert(ix.cls[1].bits[0]==UINT64_MAX&&ix.cls[1].bits[1]==UINT64_MAX&&ix.cls[1].bits[2]==1);
}

int main(void){
 mixed_class_rollback();
 struct exodus_bdi_slab s={small,raw,sr,rr,S,64,NULL};
 memset(page,0,P);
 assert(exodus_bdi_slab_attach_index(&s,&idx));
 for(size_t i=0;i<N;i++)assert(exodus_bdi_slab_import(&s,&pages[i],page));
 assert(!idx.cls[0].top&&exodus_bdi_slab_check_index(&s));
 assert(!exodus_bdi_slab_import(&s,&extra,page)&&!extra.active);
 /* Only the last summary group has room. */
 assert(exodus_bdi_slab_release(&s,&pages[N-1]));
 assert(idx.cls[0].top==128&&exodus_bdi_slab_import(&s,&extra,page));
 for(size_t i=0;i<64;i++)assert(extra.ids[i]==S-64+i);
 assert(!idx.cls[0].top);
 /* COW on full class fails without changing the bitmap or refs. */
 assert(exodus_bdi_slab_clone(&s,&pages[N-1],&extra));
 uint8_t v[4]={1,0,0,0};
 assert(!exodus_bdi_slab_write(&s,&extra,0,v)&&!idx.cls[0].top);
 assert(exodus_bdi_slab_release(&s,&pages[64]));
 assert(exodus_bdi_slab_write(&s,&extra,0,v)&&extra.ids[0]==4096);
 assert(exodus_bdi_slab_restore(&s,&pages[N-1],out)&&!memcmp(page,out,P));
 assert(exodus_bdi_slab_check_index(&s));
 for(size_t i=0;i<N;i++)if(pages[i].active)assert(exodus_bdi_slab_release(&s,&pages[i]));
 assert(exodus_bdi_slab_release(&s,&extra)&&exodus_bdi_slab_check_index(&s));
 exodus_bdi_slab_detach_index(&s);
 s.small_capacity=65;
 assert(exodus_bdi_slab_attach_index(&s,&idx));
 assert(idx.cls[0].bits[0]==UINT64_MAX&&idx.cls[0].bits[1]==1);
 assert(exodus_bdi_slab_import(&s,&extra,page));
 assert(!exodus_bdi_slab_import(&s,&pages[0],page)&&!pages[0].active);
 assert(exodus_bdi_slab_check_index(&s));
 assert(exodus_bdi_slab_release(&s,&extra));
 /* Audit detects corruption, including invalid tail bits. */
 idx.cls[0].bits[1]|=2;assert(!exodus_bdi_slab_check_index(&s));idx.cls[0].bits[1]&=~UINT64_C(2);
 assert(exodus_bdi_slab_check_index(&s));
 printf("32768-slot exhaustion, cross-group reuse, COW rollback, partial tail, mixed-class rollback and audit passed; index bytes=%zu\n",sizeof idx);
 return 0;
}
