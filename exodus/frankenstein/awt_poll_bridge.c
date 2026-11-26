/* QEMU-only AMP1 transport. Single UART owner, one SPSC slot per guest.
 * No polling wait in scheduler tick. No physical ADSP/cache proof. */
#include "amp_wait_contract.h"
#define UART ((volatile uint32_t *)(uintptr_t)0x09000000)
enum { LANES=8, IDLE=0, REQUEST=1, REPLY=2, DONE=3, FAILED=4 };
static const uint64_t gen=UINT64_C(0x1020304050607080);
static const uint64_t base=UINT64_C(0x1234567887654300);
struct lane {
    _Alignas(64) uint32_t state;
    struct ew_frame request;
    uint64_t status_va;
    uint32_t native_pid, phase;
    _Alignas(64) uint8_t q[EW_SIZE];
    _Alignas(64) uint8_t r[EW_SIZE];
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
uint64_t amp_boot(uint8_t *backing){
    memset(idle_start,0,sizeof idle_start);memset(picks,0,sizeof picks);finish_began=0;
    memset(lanes,0,sizeof lanes);cursor=pos_tx=pos_rx=0;wire=LANES;start=0;
    uint64_t hz;__asm__ volatile("mrs %0,cntfrq_el0":"=r"(hz));if(!hz)return 0;budget=hz/2;if(!budget)return 0;
    for(unsigned i=0;i<LANES;i++){lanes[i].request.generation=gen;}
    UART[0x30/4]=0;UART[0x44/4]=0x7ff;UART[0x24/4]=13;UART[0x28/4]=1;UART[0x2c/4]=0x70;UART[0x30/4]=0x301;
    __asm__ volatile("dsb sy" ::: "memory");
    /* UART handshake before any guest becomes runnable. Bounded and
     * primary-only. Host merely forwards the native kernel snapshot. */
    const char marker[]="AWT_BOOT_READY\n";
    uint64_t t=counter();
    for(unsigned i=0;i<sizeof marker-1;i++){
        while(UART[6]&(1u<<5))if(counter()-t>=budget*8)return 0;
        UART[0]=(uint8_t)marker[i];
    }
    uint8_t ids[EW_SIZE];
    for(unsigned i=0;i<EW_SIZE;i++){
        while(UART[6]&(1u<<4))if(counter()-t>=budget*8)return 0;
        uint32_t v=UART[0];if(v&0xf00u)return 0;ids[i]=(uint8_t)v;
    }
    if(memcmp(ids,"AWB1",4)||ids[4]!=1||ids[5]!=LANES||ids[6]||ids[7]||
       ew_get(ids+8,8)!=gen)return 0;
    for(unsigned i=48;i<EW_SIZE;i++)if(ids[i])return 0;
    for(unsigned i=0;i<LANES;i++){
        uint32_t pid=(uint32_t)ew_get(ids+16+4*i,4);
        if(!pid||pid>0x7fffffffu)return 0;
        for(unsigned j=0;j<i;j++)if(lanes[j].native_pid==pid)return 0;
        lanes[i].native_pid=pid;
        ew_put(backing+12288*i+8192+12,pid,4);
    }
    __asm__ volatile("dsb ish" ::: "memory");
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
    while(pos_tx<EW_SIZE && !(UART[6]&(1u<<5)))UART[0]=p->q[pos_tx++];
    if(pos_tx!=EW_SIZE)return 1;
    while(pos_rx<EW_SIZE && !(UART[6]&(1u<<4))){
        uint32_t v=UART[0];if(v&0xf00u)return 0;p->r[pos_rx++]=(uint8_t)v;
    }
    if(pos_rx==EW_SIZE){release(&p->state,REPLY);wire=LANES;}
    return 1;
}
/* Guest HVC only; one outstanding operation per lane. */
static uint64_t loadreg(const uint8_t *ctx,unsigned n){return *(const uint64_t *)(const void *)(ctx+8*n);}
static void ret(uint8_t *ctx,uint64_t result,int error){
    *(uint64_t *)(void *)ctx=result;
    uint64_t *s=(uint64_t *)(void *)(ctx+936);
    if(error)*s|=UINT64_C(1)<<29;else *s&=~(UINT64_C(1)<<29);
}
uint64_t amp_submit(uint64_t index,uint64_t cpu,uint64_t op,uint64_t arg,uint8_t *ctx){
    if(index>=LANES||cpu>=4||index/2!=cpu||op!=7)return 0;
    struct lane *p=&lanes[index];
    if(acquire(&p->state)!=IDLE)return 0;
    uint64_t ptr=loadreg(ctx,1),options=loadreg(ctx,2),usage=loadreg(ctx,3);
    if(arg!=p->native_pid||options!=1||usage){ret(ctx,22,1);return 1;}
    /* Fixed private RW ELF page mapping, not arbitrary guest page walking.
     * Only the reserved aligned four-byte result cell is writable here. */
    if(ptr!=UINT64_C(0x5008)){ret(ctx,14,1);return 1;}
    p->status_va=ptr;
    p->request=(struct ew_frame){.generation=gen,.seq=UINT64_C(0x100000000)+p->phase,
        .parent=base+index+1,.child=9+index,.kind=EW_REQUEST,.options=EW_NOHANG};
    if(!ew_encode(p->q,&p->request))return 0;
    release(&p->state,REQUEST);return 1;
}
/* Return 0/1 runnable sibling, 2 local idle, 3 fatal reference error.
 * Only the owning CPU writes a saved guest result (x8, offset 64). */
uint64_t amp_pick(uint64_t cpu,uint64_t preferred,uint8_t *contexts,uint8_t *backing){
    if(cpu>=4||preferred>=2)return 3;
    if(cpu==0&&!pump())return 3;
    unsigned mask=0;
    for(unsigned i=0;i<2;i++){
        unsigned index=(unsigned)cpu*2+i;
        struct lane *p=&lanes[index];
        uint32_t state=acquire(&p->state);
        if(state==REPLY){
            struct ew_frame result;
            if(!ew_match(&p->request,p->r,EW_SIZE,&result)||p->status_va!=0x5008){
                release(&p->state,FAILED);return 3;
            }
            uint8_t *ctx=contexts+960*index;
            if(p->phase==0 && !result.error && !result.pid){
                /* WNOHANG: do not modify status when no child is ready. */
                ret(ctx,0,0);
            }else if(p->phase==1 && !result.error && result.pid==p->native_pid){
                ew_put(backing+12288*index+8192+8,result.wait_status,4);
                ret(ctx,result.pid,0);
            }else if(p->phase==2 && result.error==EW_ECHILD){
                ret(ctx,10,1);
            }else{release(&p->state,FAILED);return 3;}
            p->phase++;
            state=p->phase==3?DONE:IDLE;release(&p->state,state);
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
    for(unsigned i=0;i<LANES;i++)if(acquire(&lanes[i].state)!=DONE)return 0;
    return 1;
}
