#include "page_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void reject(const uint8_t *p, size_t n, size_t w) {
    uint8_t out[4] = {1,2,3,4};
    CHECK(!exodus_page_read_word(p,n,w,out));
    CHECK(memcmp(out,"\1\2\3\4",4)==0);
}
/* Exercise every possible mask byte in every position, with other
 * groups occupied too. Compare first/last words against an independent
 * bytewise page construction, and reject malformed late mask bits. */
static void check_sparse_rank(void) {
    uint8_t storage[4097], expected[4096], out[4];
    uint8_t *encoded=storage+1;
    for(size_t group=0;group<8;group++){
        for(unsigned mask=0;mask<256;mask++){
            for(size_t i=0;i<8;i++)encoded[i]=(uint8_t)(i%2?0xa5:0x5a);
            encoded[group]=(uint8_t)mask;
            memset(expected,0,sizeof expected);
            size_t n=8;
            for(size_t block=0;block<64;block++){
                if(!(encoded[block/8]&(1u<<(block%8))))continue;
                for(size_t j=0;j<64;j++)
                    expected[block*64+j]=(uint8_t)(1+block*13+j);
                memcpy(encoded+n,expected+block*64,64);
                n+=64;
            }
            for(size_t block=0;block<64;block++){
                size_t w=block*16;
                CHECK(exodus_page_read_word(encoded,n,w,out));
                CHECK(!memcmp(out,expected+w*4,4));
                w+=15;
                CHECK(exodus_page_read_word(encoded,n,w,out));
                CHECK(!memcmp(out,expected+w*4,4));
            }
            encoded[7]^=0x80;
            reject(encoded,n,0);
            reject(encoded,n,1023);
        }
    }
}
int main(void) {
    check_sparse_rank();
    uint8_t payload[4096+72+17+41+8], expected[4096], out[4];
    size_t ends[5]={4096,4168,4185,4226,4234};
    size_t refs[6]={0,1,2,3,4,2}; uint64_t fingerprints[5]={0};
    struct exodus_page_dedup_store s={
        .unique={.payload=payload,.payload_capacity=sizeof(payload),.page_ends=ends,
                 .page_capacity=5,.payload_used=sizeof(payload),.page_count=5},
        .fingerprints=fingerprints,.page_refs=refs,.page_capacity=6,.page_count=6};
    memset(payload,0,sizeof(payload));
    for (size_t i=0;i<4096;i++) payload[i]=(uint8_t)(i*13);
    payload[4096+7]=0x80;
    for (size_t i=0;i<64;i++) payload[4104+i]=(uint8_t)(i+1);
    payload[4168]=2;
    for (size_t i=0;i<16;i++) payload[4169+i]=(uint8_t)(i+3);
    payload[4185]=1;
    for (size_t i=0;i<8;i++) { size_t a=4186+i*5;payload[a]=255;memset(payload+a+1,(int)(i+1),4); }
    for(size_t page=0;page<6;page++) {
        for(size_t i=0;i<4096;i++) {
            switch(refs[page]) {
            case 0: expected[i]=(uint8_t)(i*13);break;
            case 1: expected[i]=i>=4032?(uint8_t)(i-4031):0;break;
            case 2: expected[i]=(uint8_t)(i%16+3);break;
            case 3: expected[i]=(uint8_t)(i/512+1);break;
            default:expected[i]=0;break;
            }
        }
        for(size_t w=0;w<1024;w++) {
            CHECK(exodus_page_dedup_store_read_word(&s,page,w,out));
            CHECK(memcmp(out,expected+w*4,4)==0);
            CHECK(exodus_page_store_read_word(&s.unique,refs[page],w,out));
            CHECK(memcmp(out,expected+w*4,4)==0);
        }
    }
    reject(payload,0,0);reject(payload,4097,0);reject(payload,4096,1024);
    reject(payload+4096,8,0);reject(payload+4168,15,0);reject(payload+4185,39,0);
    payload[4221]=128;reject(payload+4185,41,0);payload[4221]=255;
    memset(out,0xa5,4);refs[5]=5;
    CHECK(!exodus_page_dedup_store_read_word(&s,5,0,out));
    CHECK(!exodus_page_dedup_store_read_word(&s,6,0,out));
    CHECK(!exodus_page_dedup_store_read_word(&s,0,1024,out));
    ends[4]--;CHECK(!exodus_page_dedup_store_read_word(&s,0,0,out));
    CHECK(memcmp(out,"\245\245\245\245",4)==0);
    puts("Direct read: 12288 valid reads, corruption and bounds checks passed");
    return 0;
}
