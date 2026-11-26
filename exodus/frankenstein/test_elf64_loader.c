#include "elf64_loader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { CAP=65536 };
static uint8_t dst[CAP],snapshot[CAP];
static const uint64_t lo=0x400000,hi=0x410000;
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++){p[i]=(uint8_t)v;v>>=8;}}
static void reject(const uint8_t *s,size_t n){
    struct elf64_plan p,old;
    memset(&p,0x5a,sizeof p);memcpy(&old,&p,sizeof p);
    memset(dst,0xa5,sizeof dst);memcpy(snapshot,dst,sizeof dst);
    assert(!elf64_load_image(s,n,lo,hi,dst,sizeof dst,&p));
    assert(!memcmp(dst,snapshot,sizeof dst)&&!memcmp(&p,&old,sizeof p));
}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long size=ftell(f);assert(size>0&&size<1048576);
    rewind(f);size_t n=(size_t)size;uint8_t *s=malloc(n),*m=malloc(n);assert(s&&m);
    assert(fread(s,1,n,f)==n);assert(!fclose(f));
    struct elf64_plan p;assert(elf64_load_image(s,n,lo,hi,dst,sizeof dst,&p));
    assert(p.entry==lo&&p.count==2);
    unsigned rx=0,rw=0;
    memset(snapshot,0,sizeof snapshot);
    for(size_t i=0;i<p.count;i++){
        struct elf64_segment *q=&p.seg[i];
        memcpy(snapshot+q->va-lo,s+q->file_offset,(size_t)q->filesz);
        if(q->flags==5){rx++;assert(q->filesz>=12);}
        if(q->flags==6){rw++;assert(q->memsz-q->filesz>=8192);assert(get(dst+q->va-lo,8)==UINT64_C(0x1234567887654321));}
    }
    assert(rx==1&&rw==1&&!memcmp(dst,snapshot,sizeof dst));
    uint64_t ph=get(s+32,8);assert(get(s+56,2)==2);
    #define BAD(at,v,bytes) do{memcpy(m,s,n);put(m+(at),(v),(bytes));reject(m,n);}while(0)
    for(size_t k=0;k<64;k++)reject(s,k);
    BAD(0,0,1);BAD(4,1,1);BAD(5,2,1);BAD(16,3,2);BAD(18,40,2);
    BAD(32,UINT64_MAX,8);BAD(56,65535,2);BAD(54,55,2);
    BAD(24,lo+1,8);BAD(24,hi,8);BAD(24,get(s+ph+56+16,8),8);
    BAD(ph,3,4);BAD(ph+4,7,4);BAD(ph+8,UINT64_MAX,8);
    BAD(ph+16,lo-4096,8);BAD(ph+16,UINT64_MAX-4095,8);
    BAD(ph+32,UINT64_MAX,8);BAD(ph+40,0,8);BAD(ph+40,UINT64_MAX,8);
    BAD(ph+48,1,8);BAD(ph+56+16,lo,8);
    BAD(ph+56+4,7,4);BAD(ph+56+48,3,8);
    reject(s,(size_t)ph+111);
    reject(s,(size_t)(get(s+ph+56+8,8)+get(s+ph+56+32,8)-1));
    assert(!elf64_load_image(s,n,lo,hi,dst,sizeof dst-1,&p));
    free(m);free(s);puts("ELF64_LOADER_PASS: real AArch64 ELF, RX/RW, BSS, fail-atomic malformed inputs");return 0;
}
