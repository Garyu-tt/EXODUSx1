/* R63: two separate Linux QEMU-user processes on shared host file.
 * This is a coherent reference, not physical Tegra or FreeBSD IPC.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <sched.h>
typedef struct {
    _Alignas(64) _Atomic uint32_t pub;
    uint8_t frame[24];
    _Alignas(64) _Atomic uint32_t ack;
} lane;
_Static_assert(sizeof(lane)==128 && offsetof(lane,ack)==64,"wire layout");
_Static_assert(offsetof(lane,frame)==4 && sizeof(_Atomic uint32_t)==4,"wire width");
static const uint64_t gen[2]={UINT64_C(0x123400000008),UINT64_C(0x567800000009)};
static void die(const char *why){fprintf(stderr,"FAIL: %s\n",why);exit(1);}
static uint64_t ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))die("clock");return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=8;i;){p[--i]=(uint8_t)v;v>>=8;}}
static void frame(uint8_t *b,unsigned role,uint64_t g,uint64_t seq){
    const uint8_t h[8]={'T','M','R','1',1,(uint8_t)role,0,0};
    memcpy(b,h,8);put64(b+8,g);put64(b+16,seq);
}
static void await(_Atomic uint32_t *p,uint32_t old,uint32_t next){
    uint64_t start=ms();
    for(;;){
        uint32_t n=atomic_load_explicit(p,memory_order_acquire);
        if(n==next)return;
        if(n!=old)die("unexpected token");
        if(ms()-start>=10000)die("peer timeout");
        sched_yield();
    }
}
static void send_frame(lane *l,unsigned role,uint64_t seq){
    uint32_t n=(uint32_t)seq;
    if(atomic_load_explicit(&l->pub,memory_order_relaxed)!=n-1)die("publication owner");
    await(&l->ack,n-2,n-1);
    frame(l->frame,role,gen[role-1],seq);
    atomic_store_explicit(&l->pub,n,memory_order_release);
}
static void recv_frame(lane *l,unsigned role,uint64_t seq,int bad){
    uint32_t n=(uint32_t)seq;
    if(atomic_load_explicit(&l->ack,memory_order_relaxed)!=n-1)die("ack owner");
    await(&l->pub,n-1,n);
    uint8_t expected[24];frame(expected,role,gen[role-1]+(unsigned)bad,seq);
    if(memcmp(expected,l->frame,24))die("TMR1 frame mismatch; no ACK");
    atomic_store_explicit(&l->ack,n,memory_order_release);
}
int main(int argc,char **argv){
    if(argc!=4)die("usage: binary file role normal|bad");
    unsigned role=argv[2][0]=='1'&&argv[2][1]==0?1:argv[2][0]=='2'&&argv[2][1]==0?2:0;
    if(!role)die("role");
    int bad=strcmp(argv[3],"bad")==0;
    if(!bad&&strcmp(argv[3],"normal"))die("mode");
    int fd=open(argv[1],O_RDWR);if(fd<0)die("open");
    struct stat st;if(fstat(fd,&st)||st.st_size!=4096)die("file size");
    lane *l=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(l==MAP_FAILED)die("mmap");
    close(fd);
    for(unsigned i=0;i<2;i++)if(!atomic_is_lock_free(&l[i].pub)||!atomic_is_lock_free(&l[i].ack))die("non-lockfree token");
    const uint64_t prefix=UINT64_C(0xffffffff)-500;
    for(uint64_t seq=prefix+1;seq<=prefix+1000;seq++){
        if(role==1){send_frame(l,1,seq);recv_frame(l+1,2,seq,0);}
        else {recv_frame(l,1,seq,bad);send_frame(l+1,2,seq);}
    }
    await(&l[role-1].ack,(uint32_t)(prefix+999),(uint32_t)(prefix+1000));
    if(munmap(l,4096))die("unmap");
    printf("EXODUS_CROSS_ISA_PASS role=%u pointer_bits=%zu rounds=1000\n",role,8*sizeof(void*));
    return 0;
}
