/* Host-side AArch64 reference peer; only disposable QEMU RAM file. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <sched.h>
static uint64_t ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))exit(10);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=8;i;){p[--i]=(uint8_t)v;v>>=8;}}
static void frame(uint8_t *b,unsigned role,uint64_t gen,uint64_t seq){
    const uint8_t h[8]={'T','M','R','1',1,(uint8_t)role,0,0};
    memcpy(b,h,8);put64(b+8,gen);put64(b+16,seq);
}
int main(int argc,char **argv){
    if(argc!=4||sizeof(void*)!=8)return 1;
    if(strcmp(argv[1],"ram.bin"))return 2;
    long mib=strtol(argv[2],NULL,10);
    if(mib!=32&&mib!=64)return 3;
    off_t size=(off_t)mib*1024*1024;
    int fd=open(argv[1],O_RDWR);
    struct stat st;
    if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=size)return 4;
    uint32_t *p=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,size-4096);
    if(p==MAP_FAILED)return 5;
    close(fd);
    int result=0;
    _Atomic uint32_t *pub=(_Atomic uint32_t *)&p[0], *ack=(_Atomic uint32_t *)&p[48];
    if(!atomic_is_lock_free(pub)||!atomic_is_lock_free(ack))return 11;
    const uint64_t prefix=UINT64_C(0xffffffff)-500;
    if(!strcmp(argv[3],"init")){
        memset(p,0,4096);
        atomic_init(pub,(uint32_t)prefix);atomic_init(ack,(uint32_t)prefix);
        puts("EXODUS_ARM64_RAM_INIT");
    }else if(!strncmp(argv[3],"bad-",4)){
        uint8_t req[24];
        frame(req,1,UINT64_C(0x123400000008),prefix+1);
        const char *kind=argv[3]+4;
        uint32_t token=(uint32_t)(prefix+1);
        if(!strcmp(kind,"magic"))req[0]^=1;
        else if(!strcmp(kind,"version"))req[4]^=1;
        else if(!strcmp(kind,"role"))req[5]^=3;
        else if(!strcmp(kind,"reserved"))req[7]=1;
        else if(!strcmp(kind,"generation"))req[15]^=1;
        else if(!strcmp(kind,"sequence"))req[23]^=1;
        else if(!strcmp(kind,"token"))token++;
        else return 13;
        if(atomic_load(pub)!=(uint32_t)prefix||atomic_load(ack)!=(uint32_t)prefix)return 14;
        memcpy((uint8_t*)p+64,req,24);
        atomic_store_explicit(pub,token,memory_order_release);
        puts("EXODUS_FAULT_PUBLISHED");
    }else if(!strcmp(argv[3],"check-reject")){
        uint8_t zero[24]={0};
        if(atomic_load_explicit(ack,memory_order_acquire)!=(uint32_t)prefix||
           memcmp((uint8_t*)p+128,zero,24))result=15;
        if(!result)puts("EXODUS_NO_ACK_NO_REPLY");
    }else if(!strcmp(argv[3],"exchange")){
        uint64_t deadline=ms()+30000;
        for(uint64_t seq=prefix+1;seq<=prefix+1000;seq++){
            uint32_t n=(uint32_t)seq;
            if(atomic_load_explicit(pub,memory_order_relaxed)!=n-1||
               atomic_load_explicit(ack,memory_order_acquire)!=n-1){result=6;break;}
            uint8_t req[24],reply[24];
            frame(req,1,UINT64_C(0x123400000008),seq);
            frame(reply,2,UINT64_C(0x567800000009),seq);
            memcpy((uint8_t*)p+64,req,24);
            atomic_store_explicit(pub,n,memory_order_release);
            for(;;){
                uint32_t got=atomic_load_explicit(ack,memory_order_acquire);
                if(got==n)break;
                if(got!=n-1||ms()>=deadline){result=7;break;}
                sched_yield();
            }
            if(result)break;
            if(memcmp((uint8_t*)p+128,reply,24)){result=8;break;}
        }
        if(!result)puts("EXODUS_ARM64_TMR1_PASS");
    }else result=12;
    if(munmap(p,4096))return 9;
    return result;
}
