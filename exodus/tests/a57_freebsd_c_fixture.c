/* Freestanding C fixture, built for FreeBSD/AArch64; not libc or rtld. */
#include <stdint.h>
static volatile uint64_t magic=UINT64_C(0x13579bdf2468ace0);
static volatile uint64_t lane;
static __attribute__((always_inline)) inline uint64_t call(uint64_t number,uint64_t arg,unsigned *failed){
    register uint64_t r0 __asm__("x0")=arg;
    register uint64_t r8 __asm__("x8")=number;
    unsigned carry;
    __asm__ volatile("svc #0\n\tcset %w1,cs":"+r"(r0),"=&r"(carry):"r"(r8):"x1","cc","memory");
    *failed=carry;
    return r0;
}
__attribute__((noreturn,section(".text.start"))) void _start(void){
    uint64_t id;
    __asm__ volatile("mov %0,x19":"=r"(id));
    if(!id||id>8||magic!=UINT64_C(0x13579bdf2468ace0)||lane)goto bad;
    lane=id;
    unsigned err;
    uint64_t value=call(4095,0,&err);
    if(!err||value!=78)goto bad;
    value=call(20,0,&err);
    if(err||!value||value>INT32_MAX||lane!=id||magic!=UINT64_C(0x13579bdf2468ace0))goto bad;
    (void)call(1,23,&err);
bad:
    for(;;)__asm__ volatile("svc #0x46":::"memory");
}
