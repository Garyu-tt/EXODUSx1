/* Reference-only coherent host MAP_SHARED transport. No Tegra mappings.
 * One permanent producer/consumer per direction; no live reset. */
#define _POSIX_C_SOURCE 200809L
#include "amp_execution_contract.c"
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
enum { COUNT=128, LIMIT=5000 };
struct slot {
    _Alignas(64) _Atomic uint32_t published, consumed, claimed;
    _Alignas(64) uint8_t bytes[AMP_SIZE];
};
struct mailbox { struct slot request, reply; };
_Static_assert(sizeof(_Atomic uint32_t)==4,"atomic width");
_Static_assert(offsetof(struct slot,bytes)==64,"frame offset");
_Static_assert(sizeof(struct slot)==128,"slot stride");
_Static_assert(sizeof(struct mailbox)==256,"mapping layout");
static uint64_t now(void){
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT64_MAX;
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int pause_until(uint64_t deadline){
    uint64_t n=now();if(n>=deadline)return 0;
    struct timespec t={0,100000};nanosleep(&t,NULL);return 1;
}
static int claim(struct slot *s){
    uint32_t zero=0;
    return atomic_compare_exchange_strong_explicit(&s->claimed,&zero,1,memory_order_acq_rel,memory_order_acquire);
}
static int send(struct slot *s,uint32_t seq,const uint8_t *b,uint64_t deadline){
    for(;;){
        uint32_t pub=atomic_load_explicit(&s->published,memory_order_acquire);
        uint32_t ack=atomic_load_explicit(&s->consumed,memory_order_acquire);
        if(pub!=seq-1||ack>pub||pub-ack>1)return 0;
        if(ack==pub)break;
        if(!pause_until(deadline))return 0;
    }
    memcpy(s->bytes,b,AMP_SIZE);
    atomic_store_explicit(&s->published,seq,memory_order_release);return 1;
}
static int receive(struct slot *s,uint32_t seq,uint8_t *b,uint64_t deadline){
    for(;;){
        uint32_t pub=atomic_load_explicit(&s->published,memory_order_acquire);
        if(atomic_load_explicit(&s->consumed,memory_order_relaxed)!=seq-1||pub<seq-1||pub>seq)return 0;
        if(pub==seq)break;
        if(!pause_until(deadline))return 0;
    }
    memcpy(b,s->bytes,AMP_SIZE);
    atomic_store_explicit(&s->consumed,seq,memory_order_release);return 1;
}
static int drained(struct slot *s,uint64_t deadline){
    while(atomic_load_explicit(&s->consumed,memory_order_acquire)!=COUNT+1)
        if(!pause_until(deadline))return 0;
    return 1;
}
int main(int argc,char **argv){
    if(argc!=3)return 1;
    int fd=open(argv[2],O_RDWR);struct stat st;
    if(fd<0)return 2;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=(off_t)sizeof(struct mailbox)){close(fd);return 2;}
    struct mailbox *m=mmap(NULL,sizeof *m,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);
    if(m==MAP_FAILED)return 2;
    struct slot *slots[2]={&m->request,&m->reply};
    int init_mode=!strcmp(argv[1],"init");
    for(unsigned i=0;i<2;i++){
        struct slot *s=slots[i];
        if(init_mode){atomic_init(&s->published,0);atomic_init(&s->consumed,0);atomic_init(&s->claimed,0);memset(s->bytes,0,AMP_SIZE);}
        if(!atomic_is_lock_free(&s->published)||!atomic_is_lock_free(&s->consumed)||!atomic_is_lock_free(&s->claimed))return 3;
    }
    if(init_mode){munmap(m,sizeof *m);return 0;}
    int exec=!strcmp(argv[1],"executor");
    if(!exec&&strcmp(argv[1],"authority"))return 1;
    struct slot *tx=exec?&m->request:&m->reply,*rx=exec?&m->reply:&m->request;
    if(!claim(tx))return 4;
    struct amp_session s;uint8_t q[AMP_SIZE],r[AMP_SIZE];uint64_t result;
    const uint64_t task=UINT64_C(0x1234567887654321);
    if(!init(&s,UINT64_C(0x1020304050607080),task))return 5;
    s.last=UINT64_C(0xffffffc0);
    uint64_t start=now();if(start==UINT64_MAX||start>UINT64_MAX-LIMIT)return 5;
    uint64_t deadline=start+LIMIT;
    for(uint32_t i=1;i<=COUNT+1;i++){
        if(exec){
            if(!request(&s,i==COUNT+1?AMP_EXIT_INTENT:AMP_GETPID,i==COUNT+1?23:0,q)||
               !send(tx,i,q,deadline)||!receive(rx,i,r,deadline)||
               !complete(&s,r,sizeof r,&result)||result!=(i==COUNT+1?0:task))return 6;
        }else{
            if(!receive(rx,i,q,deadline)||!serve(&s,q,sizeof q,r)||!send(tx,i,r,deadline))return 6;
            if(s.closed!=(i==COUNT+1))return 7;
        }
    }
    if(!s.closed||s.last!=UINT64_C(0x100000041)||!drained(tx,deadline))return 8;
    if(munmap(m,sizeof *m))return 9;
    printf("AMP_SHARED_PASS %s width=%zu seq=%llu\n",argv[1],sizeof(void*)*8,(unsigned long long)s.last);
    return 0;
}
