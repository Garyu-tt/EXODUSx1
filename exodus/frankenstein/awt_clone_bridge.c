/* Single-core bounded reference transport, not SMP production IPC. */
#include "amp_spawn_contract.h"
#define UART ((volatile uint32_t *)(uintptr_t)0x09000000)
static uint32_t pid;
static const uint64_t gen=UINT64_C(0x1020304050607080),task=UINT64_C(0x1234567887654301);
void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void *d,int c,size_t n){uint8_t *a=d;for(size_t i=0;i<n;i++)a[i]=(uint8_t)c;return d;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;return 0;}
static uint64_t tick(void){uint64_t x;__asm__ volatile("mrs %0,cntpct_el0":"=r"(x));return x;}
void clone_uart_init(void){
    UART[0x30/4]=0;UART[0x44/4]=0x7ff;UART[0x24/4]=13;UART[0x28/4]=1;
    UART[0x2c/4]=0x70;UART[0x30/4]=0x301;pid=0;
    __asm__ volatile("dsb sy" ::: "memory");
}
static int exchange(int spawn,uint64_t seq,struct ew_frame *out){
    struct ew_frame f={.generation=gen,.seq=seq,.parent=task,.child=9,.kind=EW_REQUEST,.options=EW_NOHANG};
    uint8_t q[EW_SIZE],r[EW_SIZE];
    if(!(spawn?aws_encode(q,&f):ew_encode(q,&f)))return 0;
    uint64_t hz;__asm__ volatile("mrs %0,cntfrq_el0":"=r"(hz));if(!hz)return 0;
    uint64_t t=tick();
    for(unsigned i=0;i<EW_SIZE;i++){
        while(UART[6]&(1u<<5))if(tick()-t>=hz*2)return 0;
        UART[0]=q[i];
    }
    for(unsigned i=0;i<EW_SIZE;i++){
        while(UART[6]&(1u<<4))if(tick()-t>=hz*2)return 0;
        uint32_t v=UART[0];if(v&0xf00u)return 0;r[i]=(uint8_t)v;
    }
    return spawn?aws_match(&f,r,sizeof r,out):ew_match(&f,r,sizeof r,out);
}
uint64_t clone_spawn(void){struct ew_frame r;if(pid||!exchange(1,1,&r))return 0;pid=r.pid;return pid;}
uint64_t clone_poll(void){struct ew_frame r;return pid&&exchange(0,UINT64_C(0x100000000),&r)&&!r.error&&!r.pid&&!r.wait_status;}
uint64_t clone_wait(uint64_t phase,uint8_t *status){
    struct ew_frame r;
    if(!pid||phase>1||!exchange(0,UINT64_C(0x100000001)+phase,&r))return 0;
    if(phase==1)return r.error==EW_ECHILD?10:0;
    if(r.error||r.pid!=pid||r.wait_status!=0x1700)return 0;
    ew_put(status,r.wait_status,4);return pid;
}
