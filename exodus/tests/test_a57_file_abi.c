#define _DEFAULT_SOURCE
#include "a57_native_file_reference.h"
#include <assert.h>
uint8_t guest_page[4096];
int file_c_test(void);
static void frame(uint8_t q[64],unsigned op,unsigned len){
    memset(q,0,64);memcpy(q,"AFI1",4);q[4]=q[5]=1;q[6]=op;q[8]=fseq+1;q[9]=len;
}
static void reject(unsigned i,unsigned val){
    uint8_t q[64],r[64],sentinel[64];unsigned seq=fseq;int fd=ffd;
    frame(q,F_OPEN,0);q[i]=val;memset(r,0xa5,64);memset(sentinel,0xa5,64);
    assert(!fio(q,r));assert(fseq==seq&&ffd==fd&&!memcmp(r,sentinel,64));
}
static void call(unsigned op,unsigned len,const char *data,unsigned err,unsigned result,const char *expected){
    uint8_t q[64],r[64];frame(q,op,len);if(data)memcpy(q+32,data,len);
    assert(fio(q,r));assert(r[5]==2&&r[7]==err&&r[10]==result&&r[8]==fseq);
    unsigned used=expected?result:0;if(expected)assert(!memcmp(r+32,expected,used));
    for(unsigned i=32+used;i<64;i++)assert(!r[i]);
    assert(!fio(q,r));
}
uint64_t file_syscall(uint64_t op,uint64_t a,uint64_t b,uint64_t c,unsigned *error){
    uint8_t q[64],r[64];static const unsigned ops[]={5,4,6,5,3,6,3};
    assert(fseq<7&&op==ops[fseq]);frame(q,(unsigned)op,0);
    if(op==5){assert(a==0x2020&&b==(fseq?0:0xa02)&&c==(fseq?0:0600));assert(!memcmp(guest_page+32,"data",5));}
    else if(op==6)assert(a==3&&!b&&!c);
    else {assert(a==3&&c==8&&b==(op==4?0x2030:0x2040));q[9]=8;if(op==4)memcpy(q+32,guest_page+48,8);}
    assert(fio(q,r));assert(r[5]==2&&r[6]==op&&r[8]==fseq);
    *error=r[7]!=0;
    if(op==3&&!*error)memcpy(guest_page+64,r+32,r[10]);
    return *error?r[7]:r[10];
}
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"c")){
        assert(file_c_test()==0&&fseq==7&&ffd==-1);
        puts("PASS: C workload through 7 native host operations");return 0;
    }
    if(argc==2&&!strcmp(argv[1],"preopen")){
        call(F_READ,8,NULL,EBADF,0,NULL);
        call(F_OPEN,0,NULL,0,3,NULL);call(F_WRITE,8,"87654321",0,8,NULL);
        call(F_CLOSE,0,NULL,0,0,NULL);call(F_OPEN,0,NULL,0,3,NULL);
        call(F_READ,8,NULL,0,8,"87654321");call(F_CLOSE,0,NULL,0,0,NULL);
        puts("PASS: pre-open EBADF recovery");return 0;
    }
    assert(argc==1);
    reject(0,'X');reject(4,2);reject(5,2);reject(6,99);reject(7,1);reject(8,2);reject(9,1);reject(10,1);reject(31,1);reject(32,1);reject(63,1);
    call(F_OPEN,0,NULL,0,3,NULL);call(F_OPEN,0,NULL,EBUSY,0,NULL);
    call(F_WRITE,8,"87654321",0,8,NULL);call(F_CLOSE,0,NULL,0,0,NULL);
    call(F_OPEN,0,NULL,0,3,NULL);call(F_WRITE,8,"XXXXXXXX",EBADF,0,NULL);
    call(F_READ,16,NULL,0,8,"87654321");call(F_READ,16,NULL,0,0,NULL);
    call(F_CLOSE,0,NULL,0,0,NULL);call(F_READ,8,NULL,EBADF,0,NULL);
    puts("PASS: 11 malformed frames, 10 file operations, replay, partial read, EOF and errors");return 0;
}
