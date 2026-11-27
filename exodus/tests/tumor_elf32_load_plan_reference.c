/* Offline ELF32/ARM load-plan validator. Synthetic addresses, no device IO.
 * Entry is virtual; resolve it through an executable PT_LOAD. Never treat
 * e_entry as a physical jump address without explicit translation.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
enum { EH=52, PH=32, PT_LOAD=1, PF_X=1, PF_W=2, PF_R=4, MAX_SEG=16 };
typedef struct {uint32_t off,va,da,filesz,memsz,flags;} segment;
typedef struct {segment seg[MAX_SEG];unsigned count;uint32_t entry_da;} plan;
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void w16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static int within(uint64_t a,uint64_t n,uint64_t base,uint64_t size){
    return n&&a>=base&&a-base<size&&n<=size-(a-base);
}
static int overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m){
    return (uint64_t)a+n>b&&(uint64_t)b+m>a;
}
static int parse(const uint8_t *b,size_t n,uint32_t base,uint32_t size,plan *out){
    plan p={0};unsigned entry_found=0;
    if(n<EH||memcmp(b,"\177ELF",4)||b[4]!=1||b[5]!=1||b[6]!=1||
       u16(b+16)!=2||u16(b+18)!=40||u32(b+20)!=1||
       u16(b+40)!=EH||u16(b+42)!=PH||
       (uint64_t)base+size>(UINT64_C(1)<<32))return 0;
    uint32_t entry=u32(b+24),phoff=u32(b+28);
    unsigned count=u16(b+44);
    if(!count||count>MAX_SEG||phoff<EH||
       phoff>n||(size_t)count*PH>n-phoff||(entry&3u))return 0;
    for(unsigned i=0;i<count;i++){
        const uint8_t *h=b+phoff+i*PH;
        if(u32(h)!=PT_LOAD)continue;
        segment s={u32(h+4),u32(h+8),u32(h+12),u32(h+16),u32(h+20),u32(h+24)};
        uint32_t align=u32(h+28);
        if(!s.memsz||s.filesz>s.memsz||s.off>n||s.filesz>n-s.off||
           !within(s.da,s.memsz,base,size)||(uint64_t)s.va+s.memsz>(UINT64_C(1)<<32)||
           !(s.flags&PF_R)||(s.flags&~7u)||((s.flags&PF_W)&&(s.flags&PF_X)))return 0;
        if(align>1&&((align&(align-1))||s.off%align!=s.va%align||s.da%align!=s.va%align))return 0;
        for(unsigned j=0;j<p.count;j++){
            segment t=p.seg[j];
            if(overlap(s.da,s.memsz,t.da,t.memsz)||overlap(s.va,s.memsz,t.va,t.memsz))return 0;
        }
        if((s.flags&PF_X)&&within(entry,4,s.va,s.filesz)){
            if(entry_found++)return 0;
            p.entry_da=s.da+(entry-s.va);
        }
        p.seg[p.count++]=s;
    }
    if(!entry_found)return 0;
    *out=p;return 1;
}
static void image(uint8_t *b){
    memset(b,0,512);memcpy(b,"\177ELF",4);b[4]=b[5]=b[6]=1;
    w16(b+16,2);w16(b+18,40);w32(b+20,1);w32(b+24,0xc0000004);
    w32(b+28,EH);w16(b+40,EH);w16(b+42,PH);w16(b+44,2);
    uint8_t *h=b+EH;
    w32(h,1);w32(h+4,256);w32(h+8,0xc0000000);w32(h+12,0x10000);
    w32(h+16,32);w32(h+20,32);w32(h+24,PF_R|PF_X);w32(h+28,4);
    h+=PH;w32(h,1);w32(h+4,320);w32(h+8,0xc0001000);w32(h+12,0x11000);
    w32(h+16,16);w32(h+20,128);w32(h+24,PF_R|PF_W);w32(h+28,4);
    memset(b+256,0xaa,32);memset(b+320,0xbb,16);
}
int main(void){
    uint8_t b[512],ram[8192];plan p;
    image(b);assert(parse(b,sizeof(b),0x10000,sizeof(ram),&p));
    assert(p.count==2&&p.entry_da==0x10004);
    memset(ram,0xcc,sizeof(ram));
    for(unsigned i=0;i<p.count;i++){
        segment s=p.seg[i];uint8_t *d=ram+s.da-0x10000;
        memcpy(d,b+s.off,s.filesz);memset(d+s.filesz,0,s.memsz-s.filesz);
    }
    assert(ram[0]==0xaa&&ram[4096]==0xbb&&ram[4112]==0&&ram[4223]==0&&ram[4224]==0xcc);
    puts("PASS: offline ELF32 ARM plan, virtual entry translation and BSS initialization");
    for(unsigned i=0;i<12;i++){
        image(b);
        switch(i){
        case 0:b[4]=2;break;
        case 1:w16(b+18,183);break;
        case 2:w32(b+28,UINT32_MAX);break;
        case 3:w32(b+EH+16,UINT32_MAX);break;
        case 4:w32(b+EH+PH+20,0x2000);break;
        case 5:w32(b+EH+PH+12,0x10000);break;
        case 6:w32(b+24,0xc0001000);break;
        case 7:w32(b+EH+24,PF_R|PF_W|PF_X);break;
        case 8:w32(b+EH+28,3);break;
        case 9:w32(b+EH+PH+8,0xc0000000);break;
        case 10:w32(b+24,0xc000001f);break;
        case 11:w32(b+EH+PH+8,0xfffffffc);break;
        }
        memset(&p,0,sizeof(p));p.entry_da=123;
        assert(!parse(b,sizeof(b),0x10000,sizeof(ram),&p)&&p.entry_da==123);
    }
    puts("PASS: twelve malformed or unauthorized ELF32 load plans rejected");
    return 0;
}
