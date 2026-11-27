/* QEMU-only freestanding FreeBSD ARMv7 adapter for pinned Tumor codec.
 * No libc, no hardware access here; mapping and barriers are in assembly.
 */
#include "ipc_native.c"
struct guest_timespec { int64_t sec; int32_t nsec; int32_t pad; };
_Static_assert(sizeof(struct guest_timespec)==16 && offsetof(struct guest_timespec,nsec)==8,"ARM EABI timespec");
extern int guest_clock(struct guest_timespec *);
static tmr_peer rx;
static int now_ms(uint64_t *out){
    struct guest_timespec t;
    if(guest_clock(&t)||t.sec<0||t.nsec<0||t.nsec>=1000000000)return 0;
    *out=(uint64_t)t.sec*1000u+(uint32_t)t.nsec/1000000u;
    return 1;
}
int guest_session_init(uint32_t session){
    uint64_t now;
    if(session>1||!now_ms(&now)||!tmr_init(&rx,1,UINT64_C(0x123400000008)+session,now))return 0;
    rx.sequence=UINT64_C(0xffffffff)-500; /* test-only rollover starting point */
    return 1;
}
/* -1: infrastructure error, 0: protocol rejected, 1: reply staged. */
int guest_reply(volatile uint8_t *page,uint32_t lo,uint32_t hi,uint32_t session){
    uint8_t req[24],reply[24];
    uint64_t now,seq=((uint64_t)hi<<32)|lo;
    if(!now_ms(&now))return -1;
    for(unsigned i=0;i<24;i++)req[i]=page[64+i];
    if(!tmr_receive(&rx,req,sizeof(req),now)||rx.sequence!=seq)return 0;
    if(!tmr_encode(reply,sizeof(reply),2,UINT64_C(0x567800000009)+session,seq))return -1;
    for(unsigned i=0;i<24;i++)page[128+i]=reply[i];
    return 1;
}
/* Minimal runtime helpers if Clang lowers struct zeroing to a call. */
void *memset(void *dst,int c,size_t n){
    volatile unsigned char *p=dst;
    for(size_t i=0;i<n;i++)p[i]=(unsigned char)c;
    return dst;
}
void __aeabi_memclr(void *dst,size_t n){(void)memset(dst,0,n);}
void __aeabi_memclr4(void *dst,size_t n){(void)memset(dst,0,n);}
void __aeabi_memclr8(void *dst,size_t n){(void)memset(dst,0,n);}
