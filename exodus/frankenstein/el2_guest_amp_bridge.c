/* QEMU-only AMP1 transport. Single UART owner, one SPSC slot per guest.
 * No polling wait in scheduler tick. No physical ADSP/cache proof. */
#include "amp_execution_contract.c"
#define UART ((volatile uint32_t *)(uintptr_t)0x09000000)
enum { LANES=8, IDLE=0, REQUEST=1, REPLY=2, DONE=3, FAILED=4 };
static const uint64_t gen=UINT64_C(0x1020304050607080);
static const uint64_t base=UINT64_C(0x1234567887654300);
struct lane {
    _Alignas(64) uint32_t state;
    struct amp_session s;
    _Alignas(64) uint8_t q[AMP_SIZE];
    _Alignas(64) uint8_t r[AMP_SIZE];
};
static struct lane lanes[LANES];
static unsigned cursor, pos_tx, pos_rx, wire;
static uint64_t start, budget;
static uint64_t idle_start[4], picks[4], finish_began;
void *memcpy(void *d,const void *p,size_t n){uint8_t *a=d;const uint8_t *b=p;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void *d,int v,size_t n){uint8_t *a=d;for(size_t i=0;i<n;i++)a[i]=(uint8_t)v;return d;}
int memcmp(const void *p,const void *q,size_t n){const uint8_t *a=p,*b=q;for(size_t i=0;i<n;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;return 0;}
static uint32_t acquire(const uint32_t *p){uint32_t x;__asm__ volatile("ldar %w0,[%1]":"=r"(x):"r"(p):"memory");return x;}
static void release(uint32_t *p,uint32_t x){__asm__ volatile("stlr %w0,[%1]"::"r"(x),"r"(p):"memory");}
static uint64_t counter(void){uint64_t x;__asm__ volatile("mrs %0,cntpct_el0":"=r"(x));return x;}
uint64_t amp_boot(void){
    memset(idle_start,0,sizeof idle_start);memset(picks,0,sizeof picks);finish_began=0;
    memset(lanes,0,sizeof lanes);cursor=pos_tx=pos_rx=0;wire=LANES;start=0;
    uint64_t hz;__asm__ volatile("mrs %0,cntfrq_el0":"=r"(hz));if(!hz)return 0;budget=hz/2;if(!budget)return 0;
    for(unsigned i=0;i<LANES;i++){if(!init(&lanes[i].s,gen,base+i+1))return 0;lanes[i].s.last=UINT64_C(0xffffffff);}
    UART[0x30/4]=0;UART[0x44/4]=0x7ff;UART[0x24/4]=13;UART[0x28/4]=1;UART[0x2c/4]=0x70;UART[0x30/4]=0x301;
    __asm__ volatile("dsb sy" ::: "memory");
    return 1;
}
/* Only CPU0 touches UART and wire state. Each tick transfers at most 128
 * bytes; backpressure returns immediately. Frame timeout is fail-closed. */
static int pump(void){
    if(wire==LANES){
        for(unsigned n=0;n<LANES;n++){
            unsigned i=(cursor+n)%LANES;
            if(acquire(&lanes[i].state)==REQUEST){wire=i;cursor=(i+1)%LANES;pos_tx=pos_rx=0;start=counter();break;}
        }
        if(wire==LANES)return 1;
    }
    struct lane *p=&lanes[wire];
    if(counter()-start>=budget)return 0;
    while(pos_tx<AMP_SIZE && !(UART[6]&(1u<<5)))UART[0]=p->q[pos_tx++];
    if(pos_tx!=AMP_SIZE)return 1;
    while(pos_rx<AMP_SIZE && !(UART[6]&(1u<<4))){
        uint32_t v=UART[0];if(v&0xf00u)return 0;p->r[pos_rx++]=(uint8_t)v;
    }
    if(pos_rx==AMP_SIZE){release(&p->state,REPLY);wire=LANES;}
    return 1;
}
/* Guest HVC only; one outstanding operation per lane. */
uint64_t amp_submit(uint64_t index,uint64_t cpu,uint64_t op,uint64_t arg){
    if(index>=LANES||cpu>=4||index/2!=cpu)return 0;
    struct lane *p=&lanes[index];
    uint8_t expected=p->s.last==UINT64_C(0xffffffff)?AMP_GETPID:AMP_EXIT_INTENT;
    if(acquire(&p->state)!=IDLE||op!=expected||arg!=(op==AMP_GETPID?0:23))return 0;
    if(!request(&p->s,(uint8_t)op,arg,p->q)){release(&p->state,FAILED);return 0;}
    release(&p->state,REQUEST);return 1;
}
/* Return 0/1 runnable sibling, 2 local idle, 3 fatal reference error.
 * Only the owning CPU writes a saved guest result (x8, offset 64). */
uint64_t amp_pick(uint64_t cpu,uint64_t preferred,uint8_t *contexts){
    if(cpu>=4||preferred>=2)return 3;
    if(cpu==0&&!pump())return 3;
    unsigned mask=0;
    for(unsigned i=0;i<2;i++){
        unsigned index=(unsigned)cpu*2+i;
        struct lane *p=&lanes[index];
        uint32_t state=acquire(&p->state);
        if(state==REPLY){
            uint64_t result,expected=p->s.pending==AMP_GETPID?p->s.task:0;
            if(!complete(&p->s,p->r,AMP_SIZE,&result)||result!=expected){release(&p->state,FAILED);return 3;}
            *(uint64_t *)(void *)(contexts+960*index+64)=result;
            state=p->s.closed?DONE:IDLE;release(&p->state,state);
        }
        if(state==IDLE||state==DONE)mask|=1u<<i;
        else if(state!=REQUEST)return 3;
    }
    if(mask){
        idle_start[cpu]=0;picks[cpu]++;
        return mask&(1u<<preferred)?preferred:preferred^1u;
    }
    uint64_t now=counter();
    if(!idle_start[cpu])idle_start[cpu]=now?now:1;
    if(now-idle_start[cpu]>=budget*8)return 3;
    return 2;
}
uint64_t amp_finish_pump(void){
    uint64_t now=counter();if(!finish_began)finish_began=now?now:1;
    return now-finish_began<budget*8 && pump();
}
uint64_t amp_all_done(void){
    if(wire!=LANES)return 0;
    for(unsigned i=0;i<LANES;i++)if(acquire(&lanes[i].state)!=DONE||!lanes[i].s.closed||lanes[i].s.last!=UINT64_C(0x100000001))return 0;
    return 1;
}
