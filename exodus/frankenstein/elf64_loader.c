#include "elf64_loader.h"
#include <string.h>
static uint64_t rd(const uint8_t *p,unsigned n){
    uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;
}
static int span(uint64_t p,uint64_t n,uint64_t end){return p<=end&&n<=end-p;}
static int aligned(uint64_t x){return (x&(ELF64_PAGE-1))==0;}
static int round_page(uint64_t x,uint64_t *out){
    if(x>UINT64_MAX-(ELF64_PAGE-1))return 0;
    *out=(x+(ELF64_PAGE-1))&~(uint64_t)(ELF64_PAGE-1);return 1;
}
int elf64_plan_image(const uint8_t *s,size_t n,uint64_t lo,uint64_t hi,struct elf64_plan *out){
    if(!s||!out||n<64||lo>=hi||!aligned(lo)||!aligned(hi))return 0;
    /* ELF64 little-endian current version, ET_EXEC, EM_AARCH64. */
    if(memcmp(s,"\177ELF",4)||s[4]!=2||s[5]!=1||s[6]!=1||
       rd(s+16,2)!=2||rd(s+18,2)!=183||rd(s+20,4)!=1||
       rd(s+48,4)!=0||rd(s+52,2)!=64||rd(s+54,2)!=56)return 0;
    uint64_t off=rd(s+32,8),num=rd(s+56,2);
    if(!num||num>64||off<64||!span(off,num*56,n))return 0;
    struct elf64_plan p={0};p.entry=rd(s+24,8);
    if(p.entry&3)return 0;
    unsigned executable_entry=0;
    for(uint64_t i=0;i<num;i++){
        const uint8_t *h=s+off+56*i;uint64_t type=rd(h,4);
        /* Deliberately strict: only PT_NULL, LOAD, GNU_STACK (NX), NOTE, PHDR. */
        if(type==0||type==4||type==6)continue;
        uint32_t f=(uint32_t)rd(h+4,4);
        if(type==UINT64_C(0x6474e551)){if(f!=6)return 0;continue;}
        if(type!=1||p.count==ELF64_MAX_SEGMENTS)return 0;
        uint64_t fo=rd(h+8,8),va=rd(h+16,8),fs=rd(h+32,8),ms=rd(h+40,8),a=rd(h+48,8),end;
        if(!ms||fs>ms||!span(fo,fs,n)||va<lo||!span(va,ms,hi)||
           !aligned(va)||!aligned(fo)||a!=ELF64_PAGE||!round_page(va+ms,&end)||end>hi||
           (f!=ELF64_R&&f!=(ELF64_R|ELF64_X)&&f!=(ELF64_R|ELF64_W)))return 0;
        for(size_t j=0;j<p.count;j++){
            const struct elf64_segment *q=&p.seg[j];uint64_t qe;
            if(!round_page(q->va+q->memsz,&qe)||!(end<=q->va||qe<=va))return 0;
        }
        if((f&ELF64_X)&&p.entry>=va&&p.entry-va<fs&&fs-(p.entry-va)>=4)executable_entry++;
        p.seg[p.count++]=(struct elf64_segment){va,fo,fs,ms,f};
    }
    if(!p.count||executable_entry!=1)return 0;
    *out=p;return 1;
}
int elf64_load_image(const uint8_t *s,size_t n,uint64_t lo,uint64_t hi,uint8_t *backing,size_t cap,struct elf64_plan *out){
    struct elf64_plan p;
    if(!out||!backing||!elf64_plan_image(s,n,lo,hi,&p)||hi-lo!=cap)return 0;
    memset(backing,0,cap);
    for(size_t i=0;i<p.count;i++){
        const struct elf64_segment *q=&p.seg[i];
        memcpy(backing+(size_t)(q->va-lo),s+(size_t)q->file_offset,(size_t)q->filesz);
    }
    *out=p;return 1;
}
