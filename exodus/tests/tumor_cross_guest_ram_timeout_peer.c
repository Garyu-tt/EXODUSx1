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
#include "ipc_native.c"
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
    _Atomic uint32_t *cmd=(_Atomic uint32_t *)&p[64], *state=(_Atomic uint32_t *)&p[80];
    const uint64_t prefix=UINT64_C(0xffffffff)-500;
    if(!atomic_is_lock_free(cmd)||!atomic_is_lock_free(state))return 16;
    if(!strcmp(argv[3],"init")){
        memset(p,0,4096);
        atomic_init(pub,(uint32_t)prefix);atomic_init(ack,(uint32_t)prefix);
        atomic_init(cmd,0);atomic_init(state,0);
        puts("EXODUS_ARM64_RAM_INIT");
    }else if(!strcmp(argv[3],"check-stall")){
        uint8_t request[24],reply[24];
        uint64_t seq=prefix+10;
        if(!tmr_encode(request,24,1,UINT64_C(0x123400000008),seq)||
           !tmr_encode(reply,24,2,UINT64_C(0x567800000009),seq))return 24;
        if(atomic_load(pub)!=(uint32_t)seq||atomic_load(ack)!=(uint32_t)seq||
           atomic_load(cmd)!=0||atomic_load(state)!=0||
           memcmp((uint8_t*)p+64,request,24)||memcmp((uint8_t*)p+128,reply,24))result=25;
        if(!result)puts("EXODUS_STALL_NO_SPURIOUS_ACK");
    }else if(!strcmp(argv[3],"exchange")||!strcmp(argv[3],"stall")){
        int stall=!strcmp(argv[3],"stall");
        uint64_t deadline=ms()+30000;
        for(unsigned session=0;session<2;session++){
            unsigned rounds=session?1000:(stall?10:11);
            tmr_peer rx;
            if(!tmr_init(&rx,2,UINT64_C(0x567800000009)+session,ms())){result=21;goto done;}
            /* Fixture starts near rollover; not an on-wire resumption mechanism. */
            rx.sequence=prefix;
            for(uint64_t seq=prefix+1;seq<=prefix+rounds;seq++){
                uint32_t n=(uint32_t)seq;
                if(atomic_load_explicit(pub,memory_order_relaxed)!=n-1||
                   atomic_load_explicit(ack,memory_order_acquire)!=n-1){result=6;goto done;}
                uint8_t req[24],reply[24];
                if(!tmr_encode(req,sizeof(req),1,UINT64_C(0x123400000008)+session,seq)){result=22;goto done;}
                if(!session&&seq==prefix+11){
                    /* Corrupt generation after ten completed round trips. */
                    req[15]^=1;
                    memcpy((uint8_t*)p+64,req,24);
                    atomic_store_explicit(pub,n,memory_order_release);
                    while(atomic_load_explicit(state,memory_order_acquire)!=1){
                        if(ms()>=deadline){result=17;goto done;}
                        sched_yield();
                    }
                    if(!tmr_encode(reply,sizeof(reply),2,UINT64_C(0x567800000009),seq-1)){result=23;goto done;}
                    if(atomic_load_explicit(ack,memory_order_acquire)!=n-1||
                       memcmp((uint8_t*)p+128,reply,24)){result=18;goto done;}
                    /* Guest is quiescent. Reset only host-owned fields. */
                    memset((uint8_t*)p+64,0,24);
                    atomic_store_explicit(pub,(uint32_t)prefix,memory_order_relaxed);
                    atomic_store_explicit(cmd,1,memory_order_release);
                    while(atomic_load_explicit(state,memory_order_acquire)!=2){
                        if(ms()>=deadline){result=19;goto done;}
                        sched_yield();
                    }
                    uint8_t zero[24]={0};
                    if(atomic_load_explicit(ack,memory_order_acquire)!=(uint32_t)prefix||
                       memcmp((uint8_t*)p+128,zero,24)){result=20;goto done;}
                    break;
                }
                memcpy((uint8_t*)p+64,req,24);
                atomic_store_explicit(pub,n,memory_order_release);
                for(;;){
                    uint32_t got=atomic_load_explicit(ack,memory_order_acquire);
                    if(got==n)break;
                    if(got!=n-1||ms()>=deadline){result=7;goto done;}
                    sched_yield();
                }
                memcpy(reply,(uint8_t*)p+128,sizeof(reply));
                if(!tmr_receive(&rx,reply,sizeof(reply),ms())||rx.sequence!=seq){result=8;goto done;}
            }
            if(stall){puts("EXODUS_ARM64_STOPPED_AFTER_10");goto done;}
        }
        puts("EXODUS_ARM64_NATIVE_CODEC_PASS");
    }else result=12;
done:
    if(munmap(p,4096))return 9;
    return result;
}
