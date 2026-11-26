#include "elf64_loader.h"
#include <stdint.h>
#include <string.h>
extern const uint8_t _binary_elf_input_elf_start[],_binary_elf_input_elf_end[];
enum { GUESTS=8, WINDOW=12288 };
uint64_t elf_boot(uint8_t *backing){
    uintptr_t start=(uintptr_t)_binary_elf_input_elf_start,end=(uintptr_t)_binary_elf_input_elf_end;
    if(end<=start)return 0;
    size_t n=(size_t)(end-start);
    struct elf64_plan p;
    if(!elf64_plan_image(_binary_elf_input_elf_start,n,0x3000,0x6000,&p)||p.count!=2||p.entry!=0x3000)return 0;
    unsigned rx=0,rw=0;
    for(size_t i=0;i<p.count;i++){
        const struct elf64_segment *q=&p.seg[i];
        if(q->va==0x3000&&q->flags==5&&q->memsz<=4096)rx++;
        else if(q->va==0x5000&&q->flags==6&&q->filesz==8&&q->memsz==16)rw++;
        else return 0;
    }
    if(rx!=1||rw!=1)return 0;
    for(unsigned i=0;i<GUESTS;i++){
        uint8_t *b=backing+i*WINDOW;
        memset(b,0xa5,WINDOW);
        if(!elf64_load_image(_binary_elf_input_elf_start,n,0x3000,0x6000,b,WINDOW,&p))return 0;
    }
    /* Clean data and invalidate instructions before sharing executable bytes.
     * QEMU fixture; line strides are obtained from CTR_EL0. */
    uint64_t ctr;__asm__ volatile("mrs %0,ctr_el0":"=r"(ctr));
    uintptr_t dline=(uintptr_t)4<<((ctr>>16)&15),iline=(uintptr_t)4<<(ctr&15);
    uintptr_t lo=(uintptr_t)backing,hi=lo+GUESTS*WINDOW;
    for(uintptr_t paddr=lo&~(dline-1);paddr<hi;paddr+=dline)__asm__ volatile("dc cvau,%0"::"r"(paddr):"memory");
    __asm__ volatile("dsb ish":::"memory");
    for(uintptr_t paddr=lo&~(iline-1);paddr<hi;paddr+=iline)__asm__ volatile("ic ivau,%0"::"r"(paddr):"memory");
    __asm__ volatile("dsb ish; isb":::"memory");
    return 1;
}
uint64_t elf_verify(const uint8_t *backing){
    for(unsigned i=0;i<GUESTS;i++){
        uint64_t magic,id;const uint8_t *p=backing+i*WINDOW+8192;
        memcpy(&magic,p,8);memcpy(&id,p+8,8);
        if(magic!=UINT64_C(0x13579bdf2468ace0)||id!=i+1)return 0;
    }
    return 1;
}
