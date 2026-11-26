/* QEMU virt serial transport, NOT shared RAM or physical Tegra.
 * Exact existing AMP1 codec; single serialized EL2 fixture. */
#include "amp_execution_contract.c"
#define UART ((volatile uint32_t *)(uintptr_t)0x09000000)
#define ERR (UINT64_MAX-1)
static struct amp_session s;
void *memcpy(void *d,const void *p,size_t n){uint8_t *a=d;const uint8_t *b=p;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void *d,int v,size_t n){uint8_t *a=d;for(size_t i=0;i<n;i++)a[i]=(uint8_t)v;return d;}
int memcmp(const void *p,const void *q,size_t n){const uint8_t *a=p,*b=q;for(size_t i=0;i<n;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;return 0;}
static uint64_t counter(void){uint64_t x;__asm__ volatile("mrs %0, cntpct_el0":"=r"(x));return x;}
static uint64_t freq(void){uint64_t x;__asm__ volatile("mrs %0, cntfrq_el0":"=r"(x));return x;}
static void uart_setup(void){
    /* QEMU PL011 only. Establish a defined RX/TX/FIFO configuration. */
    UART[0x30/4]=0;
    UART[0x44/4]=0x7ff;
    UART[0x24/4]=13;
    UART[0x28/4]=1;
    UART[0x2c/4]=0x70;
    UART[0x30/4]=0x301;
    __asm__ volatile("dsb sy" ::: "memory");
}
static int wait_flag(uint32_t flag,uint64_t start,uint64_t budget){
    while(UART[6]&flag)if(counter()-start>=budget)return 0;
    return counter()-start<budget;
}
uint64_t bridge_init(void){
    if(!init(&s,UINT64_C(0x1020304050607080),UINT64_C(0x1234567887654321)))return 0;
    s.last=UINT64_C(0xffffffc0);
    uart_setup();
    const char ready[]="EXODUS_EL2_AMP_READY\n";
    uint64_t start=counter(),hz=freq();
    if(!hz||hz>UINT64_MAX/5)return 0;
    for(size_t i=0;i<sizeof ready-1;i++){
        if(!wait_flag(1u<<5,start,hz*5))return 0;
        UART[0]=(uint8_t)ready[i];
    }
    return 1;
}
uint64_t bridge_call(uint64_t op,uint64_t arg){
    uint8_t q[AMP_SIZE],r[AMP_SIZE];uint64_t result,start=counter(),hz=freq();
    if(!hz||hz>UINT64_MAX/5||op>UINT8_MAX||!request(&s,(uint8_t)op,arg,q))goto bad;
    for(size_t i=0;i<AMP_SIZE;i++){if(!wait_flag(1u<<5,start,hz*5))goto bad;UART[0]=q[i];}
    for(size_t i=0;i<AMP_SIZE;i++){if(!wait_flag(1u<<4,start,hz*5))goto bad;uint32_t v=UART[0];if(v&0xf00u)goto bad;r[i]=(uint8_t)v;}
    if(!complete(&s,r,sizeof r,&result)){fault(&s);return UINT64_MAX-2;}
    return result;
bad: fault(&s);return ERR;
}
