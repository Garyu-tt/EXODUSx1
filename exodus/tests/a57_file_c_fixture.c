/* R87: C guest on existing R80 file ABI. Fixed test addresses, not libc. */
#include <stdint.h>
#ifdef FIO_HOST_TEST
extern uint8_t guest_page[4096];
extern uint64_t file_syscall(uint64_t,uint64_t,uint64_t,uint64_t,unsigned *);
#define CELL(address) (guest_page+((address)-0x2000u))
#else
#define CELL(address) ((volatile uint8_t *)(uintptr_t)(address))
static inline uint64_t file_syscall(uint64_t number,uint64_t a,uint64_t b,uint64_t c,unsigned *error){
    register uint64_t x0 __asm__("x0")=a;
    register uint64_t x1 __asm__("x1")=b;
    register uint64_t x2 __asm__("x2")=c;
    register uint64_t x8 __asm__("x8")=number;
    unsigned carry;
    __asm__ volatile("svc #0\n\tcset %w4,cs":"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x8),"=&r"(carry)::"cc","memory");
    *error=carry;
    return x0;
}
#endif
static int file_c_run(void){
    volatile uint8_t *p=CELL(0x2020),*w=CELL(0x2030),*r=CELL(0x2040);
    p[0]='d';p[1]='a';p[2]='t';p[3]='a';p[4]=0;
    for(unsigned i=0;i<8;i++){w[i]=(uint8_t)('8'-i);r[i]=0xa5;}
    unsigned err;
    uint64_t fd=file_syscall(5,0x2020,0xa02,0600,&err);
    if(err||fd!=3)return 1;
    if(file_syscall(4,fd,0x2030,8,&err)!=8||err)return 2;
    if(file_syscall(6,fd,0,0,&err)||err)return 3;
    fd=file_syscall(5,0x2020,0,0,&err);
    if(err||fd!=3)return 4;
    if(file_syscall(3,fd,0x2040,8,&err)!=8||err)return 5;
    for(unsigned i=0;i<8;i++)if(r[i]!=w[i])return 6;
    if(file_syscall(6,fd,0,0,&err)||err)return 7;
    if(file_syscall(3,fd,0x2040,8,&err)!=9||!err)return 8;
    for(unsigned i=0;i<8;i++)if(r[i]!=w[i])return 9;
    return 0;
}
#ifdef FIO_HOST_TEST
int file_c_test(void){return file_c_run();}
#else
__attribute__((section(".text.entry"),noreturn)) void file_c_entry(void){
    unsigned err;
    int result=file_c_run();
    (void)file_syscall(result?4090:4089,(uint64_t)result,0,0,&err);
    for(;;)__asm__ volatile("svc #0x46":::"memory");
}
#endif
