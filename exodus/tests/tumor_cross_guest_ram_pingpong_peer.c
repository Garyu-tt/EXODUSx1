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
    if(!strcmp(argv[3],"init")){
        memset(p,0,4096);
        atomic_init(pub,0);atomic_init(ack,0);
        puts("EXODUS_ARM64_RAM_INIT");
    }else if(!strcmp(argv[3],"exchange")){
        uint64_t deadline=ms()+30000;
        for(uint32_t seq=1;seq<=1000;seq++){
            if(atomic_load_explicit(pub,memory_order_relaxed)!=seq-1||
               atomic_load_explicit(ack,memory_order_acquire)!=seq-1){result=6;break;}
            p[16]=seq;p[17]=~seq;
            atomic_store_explicit(pub,seq,memory_order_release);
            for(;;){
                uint32_t n=atomic_load_explicit(ack,memory_order_acquire);
                if(n==seq)break;
                if(n!=seq-1||ms()>=deadline){result=7;break;}
                sched_yield();
            }
            if(result)break;
            if(p[32]!=seq+42||p[33]!=~(seq+42)){result=8;break;}
        }
        if(!result)puts("EXODUS_ARM64_RAM_1000_PASS");
    }else result=12;
    if(munmap(p,4096))return 9;
    return result;
}
