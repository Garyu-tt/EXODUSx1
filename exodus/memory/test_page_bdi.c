#include "page_read.h"
#include "page_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define C(x) do { if(!(x)) {fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
enum { P=EXODUS_PAGE_BYTES };
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint8_t p[P], q[P], enc[P], out[P], frame[EXODUS_SNAPSHOT_MAX_BYTES], payload[2*P];
int main(void){
 for(size_t b=0;b<64;b++) for(size_t w=0;w<16;w++)
  put(p+b*64+w*4,UINT32_MAX-255-(uint32_t)b*1024+(uint32_t)w*17);
 size_t n=exodus_page_encode_adaptive(p,enc);
 C(n==1289&&enc[0]==EXODUS_PAGE_BDI_VERSION);
 C(exodus_page_decode(enc,n,out)&&!memcmp(p,out,P));
 for(size_t i=0;i<P/4;i++){uint8_t v[4];C(exodus_page_read_word(enc,n,i,v));C(!memcmp(v,p+4*i,4));}
 memset(out,0xa5,P);C(!exodus_page_decode(enc,n-2,out));for(size_t i=0;i<P;i++)C(out[i]==0xa5);
 uint8_t v[4]={1,2,3,4};C(!exodus_page_read_word(enc,n+2,0,v)&&v[0]==1);
 memcpy(q,p,P);put(q+64,0);n=exodus_page_encode_adaptive(q,enc);
 C(n==1333&&enc[0]==EXODUS_PAGE_BDI_VERSION);
 C(exodus_page_decode(enc,n,out)&&!memcmp(q,out,P));
 for(size_t i=0;i<P/4;i++){C(exodus_page_read_word(enc,n,i,v));C(!memcmp(v,q+4*i,4));}
 size_t ends[2],refs[2];uint64_t hashes[2];
 struct exodus_page_dedup_store s={.unique={.payload=payload,.payload_capacity=sizeof(payload),.page_ends=ends,.page_capacity=2},.fingerprints=hashes,.page_refs=refs,.page_capacity=2};
 C(exodus_page_dedup_store_append_adaptive(&s,p));C(exodus_page_dedup_store_append_adaptive(&s,p));C(s.unique.payload_used==1289);
 C(exodus_page_dedup_store_write_word(&s,1,16,q+64));C(s.unique.page_count==2);
 C(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(p,out,P));
 C(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(q,out,P));
 C(exodus_page_dedup_store_replace_adaptive(&s,1,p)&&s.unique.page_count==1&&s.unique.payload_used==1289);
 size_t f=exodus_snapshot_encode(p,NULL,frame);C(exodus_snapshot_decode(frame,f,NULL,out)&&!memcmp(p,out,P));
 f=exodus_snapshot_encode(q,p,frame);C(exodus_snapshot_decode(frame,f,p,out)&&!memcmp(q,out,P));
 puts("BDI: 4096 -> 1289 bytes; 2048 direct reads, mixed blocks, COW and snapshots passed");
 return 0;
}
