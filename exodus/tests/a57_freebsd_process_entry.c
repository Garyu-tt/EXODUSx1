/* Reference C process entry: private initial stack, argv/env/auxv.
 * No libc or dynamic loader; exit/getpid use the existing ABI. */
#include <stdint.h>
static volatile uint64_t magic=UINT64_C(0x13579bdf2468ace0);
static volatile uint64_t lane;
static __attribute__((always_inline)) inline uint64_t call(uint64_t number,unsigned *failed){
    register uint64_t r0 __asm__("x0")=0;
    register uint64_t r8 __asm__("x8")=number;
    unsigned carry;
    __asm__ volatile("svc #0\n\tcset %w1,cs":"+r"(r0),"=&r"(carry):"r"(r8):"x1","cc","memory");
    *failed=carry;
    return r0;
}
static int eq(const char *p,const char *q){
    while(*p && *p==*q){++p;++q;}
    return *p==*q;
}
__attribute__((noinline)) uint64_t exodus_main(const uint64_t *s,uint64_t id){
    if((uintptr_t)s!=0x5f00||!id||id>8||magic!=UINT64_C(0x13579bdf2468ace0)||lane)return 99;
    if(s[0]!=2||s[1]!=0x5f60||s[2]!=0x5f70||s[3]||s[4]!=0x5f80||s[5]||
       s[6]!=6||s[7]!=4096||s[8]||s[9])return 99;
    if(!eq((const char *)(uintptr_t)s[1],"exodus")||
       !eq((const char *)(uintptr_t)s[4],"MODE=TEST"))return 99;
    const char *arg=(const char *)(uintptr_t)s[2];
    if(arg[0]!='l'||arg[1]!='a'||arg[2]!='n'||arg[3]!='e'||arg[4]!='-'||
       arg[5]!=(char)('0'+id)||arg[6])return 99;
    lane=id;
    unsigned err;
    uint64_t value=call(4095,&err);
    if(!err||value!=78)return 99;
    value=call(20,&err);
    if(err||!value||value>INT32_MAX||lane!=id||magic!=UINT64_C(0x13579bdf2468ace0))return 99;
    return 23;
}
__asm__(".section .text.start,\"ax\"\n"
        ".global _start\n_start:\n"
        "mov x0,sp\nmov x1,x19\n"
        "stp x18,x30,[sp,#-16]!\nbl exodus_main\n"
        "ldp x18,x30,[sp],#16\n"
        "cmp x0,#23\nb.ne 1f\nmov x8,#1\nsvc #0\n"
        "1: svc #0x46\nb 1b\n.previous\n");
