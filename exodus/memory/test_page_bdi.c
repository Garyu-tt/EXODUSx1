#include "page_read.h"
#include "page_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define C(x) do { if(!(x)) {fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
enum { P=EXODUS_PAGE_BYTES };
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint8_t p[P], q[P], enc[P], out[P], frame[EXODUS_SNAPSHOT_MAX_BYTES], payload[2*P];
static uint32_t rng=0x5ca92d1u;
static uint32_t next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void test_incremental(void){
 uint8_t raw[P], changed[4], old[4];
 for(size_t w=0;w<P/4;w++)put(raw+w*4,next());
 uint64_t h=exodus_page_fingerprint(raw);
 for(size_t i=0;i<10000;i++){
  size_t w=next()%(P/4);memcpy(old,raw+4*w,4);put(changed,next());
  uint64_t newhash,undo;
  C(exodus_page_fingerprint_update(h,w,old,changed,&newhash));
  C(exodus_page_fingerprint_update(newhash,w,changed,old,&undo)&&undo==h);
  memcpy(raw+4*w,changed,4);
  C(newhash==exodus_page_fingerprint(raw));h=newhash;
 }
 uint64_t sentinel=123;
 C(!exodus_page_fingerprint_update(h,P/4,old,changed,&sentinel)&&sentinel==123);
 C(!exodus_page_fingerprint_update(h,0,NULL,changed,&sentinel)&&sentinel==123);
 memset(raw,0,P);uint64_t zero=exodus_page_fingerprint(raw);
 put(raw,1);uint64_t first=exodus_page_fingerprint(raw);
 put(raw,0);put(raw+4,1);C(exodus_page_fingerprint(raw)!=first&&first!=zero);
 puts("Fingerprint v2: 10000 incremental/full comparisons and reversals passed");
}
/* Contiguous zero blocks, then compact nonzero blocks, then RAW blocks
 * (or reversed RAW-first order). All nonzero words differ from neighbors:
 * word-run overhead cannot beat the sparse/RAW fallback in this fixture.
 * Compare exact fallback bytes, and exact canonical BDI bytes when it wins. */
static void test_bdi_early_reject(void){
 uint8_t page[P], wire[P], fallback[P], expected[P], decoded[P];
 for(unsigned reverse=0;reverse<2;reverse++){
  for(size_t zero=0;zero<=64;zero++){
   for(size_t compact=0;compact<=64-zero;compact++){
    memset(page,0,P);
    for(size_t b=0;b<64-zero;b++){
     size_t block=reverse?b:zero+b;
     bool packed=reverse?b>=64-zero-compact:b<compact;
     for(size_t w=0;w<16;w++)
      put(page+64*block+4*w,1000u+(uint32_t)block*4096u+(uint32_t)w*(packed?1u:257u));
    }
    size_t best=exodus_page_encode(page,fallback);
    size_t bdi=9+64*64-(zero+compact)*44;
    size_t n=exodus_page_encode_adaptive(page,wire);
    C(n==(bdi<best?bdi:best));
    if(bdi>=best){C(!memcmp(wire,fallback,best));}
    else{
     memset(expected,0,9);expected[0]=EXODUS_PAGE_BDI_VERSION;
     size_t pos=9;
     for(size_t block=0;block<64;block++){
      bool z=reverse?block>=64-zero:block<zero;
      bool c=reverse?block>=64-zero-compact&&block<64-zero:
                       block>=zero&&block<zero+compact;
      if(z||c){
       expected[1+block/8]|=(uint8_t)(1u<<(block%8));
       memcpy(expected+pos,page+64*block,4);
       for(size_t w=0;w<16;w++)expected[pos+4+w]=(uint8_t)(z?0:w);
       pos+=20;
      }else{memcpy(expected+pos,page+64*block,64);pos+=64;}
     }
     C(pos==n&&!memcmp(wire,expected,n));
    }
    C(exodus_page_decode(wire,n,decoded)&&!memcmp(page,decoded,P));
   }
  }
 }
}
int main(void){
 test_bdi_early_reject();
 test_incremental();
 for(size_t b=0;b<64;b++) for(size_t w=0;w<16;w++)
  put(p+b*64+w*4,UINT32_MAX-255-(uint32_t)b*1024+(uint32_t)w*17);
 size_t n=exodus_page_encode_adaptive(p,enc);
 C(n==1289&&enc[0]==EXODUS_PAGE_BDI_VERSION);
 uint64_t bdi_hash;C(exodus_page_bdi_fingerprint(enc,n,&bdi_hash)&&bdi_hash==exodus_page_fingerprint(p));
 C(exodus_page_decode(enc,n,out)&&!memcmp(p,out,P));
 for(size_t i=0;i<P/4;i++){uint8_t v[4];C(exodus_page_read_word(enc,n,i,v));C(!memcmp(v,p+4*i,4));}
 memset(out,0xa5,P);C(!exodus_page_decode(enc,n-2,out));for(size_t i=0;i<P;i++)C(out[i]==0xa5);
 uint8_t v[4]={1,2,3,4};C(!exodus_page_read_word(enc,n+2,0,v)&&v[0]==1);
 memcpy(q,p,P);put(q+64,0);n=exodus_page_encode_adaptive(q,enc);
 C(n==1333&&enc[0]==EXODUS_PAGE_BDI_VERSION);
 C(exodus_page_bdi_fingerprint(enc,n,&bdi_hash)&&bdi_hash==exodus_page_fingerprint(q));
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
 
 /* A shared page splits and merges via direct word writes. Rebase, RAW
  * transition, hash equivalence and failure atomicity. */
 memcpy(q,p,P);put(q,UINT32_MAX-254);
 C(exodus_page_dedup_store_write_word(&s,1,0,q));
 C(s.unique.page_count==2&&s.unique.payload_used==2578);
 C(hashes[refs[1]]==exodus_page_fingerprint(q));
 C(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(q,out,P));
 C(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(p,out,P));
 C(exodus_page_dedup_store_write_word(&s,1,0,p));
 C(s.unique.page_count==1&&s.unique.payload_used==1289);
 put(q,0);
 size_t oldcap=s.unique.payload_capacity;
 s.unique.payload_capacity=s.unique.payload_used;
 uint8_t before[sizeof(payload)];memcpy(before,payload,sizeof(payload));
 uint64_t h=hashes[0];
 C(!exodus_page_dedup_store_write_word(&s,1,0,q));
 C(s.unique.page_count==1&&s.unique.payload_used==1289&&hashes[0]==h&&!memcmp(before,payload,sizeof(payload)));
 s.unique.payload_capacity=oldcap;
 C(exodus_page_dedup_store_write_word(&s,1,0,q));
 C(s.unique.payload_used==1289+1333);
 C(hashes[refs[1]]==exodus_page_fingerprint(q));
 C(exodus_page_dedup_store_write_word(&s,1,0,p));
 C(s.unique.page_count==1&&s.unique.payload_used==1289);
 /* Merge through a real matching candidate. */
 put(q,UINT32_MAX-254);
 C(exodus_page_dedup_store_write_word(&s,1,0,q));
 uint64_t actual=hashes[0],fake;
 /* Force a collision with the prospective new contents of page 1. */
 put(q,UINT32_MAX-253);
 size_t encoded_size=exodus_page_encode_adaptive(q,enc);
 C(exodus_page_bdi_fingerprint(enc,encoded_size,&fake));
 hashes[0]=fake;
 C(exodus_page_dedup_store_write_word(&s,1,0,q));
 C(s.unique.page_count==2);
 C(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(p,out,P));
 C(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(q,out,P));
 hashes[0]=actual;
 C(exodus_page_dedup_store_write_word(&s,0,0,q));
 C(s.unique.page_count==1&&s.unique.payload_used==1289);
 C(exodus_page_dedup_store_restore(&s,0,out)&&!memcmp(q,out,P));
 C(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(q,out,P));
 /* Last BDI block expands: fall back to RAW with correct logical bytes. */
 for(size_t b=0;b<63;b++){put(q+b*64,0);C(exodus_page_dedup_store_write_word(&s,1,b*16,q+b*64));}
 C(s.unique.page_count==2);
 put(q+63*64,0);C(exodus_page_dedup_store_write_word(&s,1,63*16,q+63*64));
 C(exodus_page_dedup_store_restore(&s,1,out)&&!memcmp(q,out,P));
 C(hashes[refs[1]]==exodus_page_fingerprint(q));
 puts("BDI patch: rebase, RAW transition, shared writes, merge and budget rollback passed");
 puts("BDI: 4096 -> 1289 bytes; 2048 direct reads, mixed blocks, COW and snapshots passed");
 return 0;
}
