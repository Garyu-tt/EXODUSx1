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
int main(void){
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
 printf("32768-slot exhaustion, cross-group reuse, COW rollback, partial tail and audit passed; index bytes=%zu\n",sizeof idx);
 return 0;
}
