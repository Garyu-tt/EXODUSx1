/* Execute a real toolchain-generated ARM ELF32 fixture on Unicorn only.
 * Uses R29's restrictive parser and explicitly loads device addresses.
 * This is not a kernel, MMU, ADSP or FreeBSD boot proof.
 */
#define main synthetic_parser_selftest
#include "tumor_elf32_load_plan_reference.c"
#undef main
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <stdlib.h>
static void check_uc(uc_err e){
    if(e!=UC_ERR_OK){fprintf(stderr,"Unicorn: %s\n",uc_strerror(e));exit(1);}
}
int main(int argc,char **argv){
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long length=ftell(f);
    assert(length>0&&length<=1048576&&!fseek(f,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)length);assert(bytes);
    assert(fread(bytes,1,(size_t)length,f)==(size_t)length);assert(!fclose(f));
    plan p;uint8_t ram[8192];
    assert(parse(bytes,(size_t)length,0x10000,sizeof(ram),&p));
    assert(p.count==2&&p.entry_da==0x10000);
    memset(ram,0xcc,sizeof(ram));
    for(unsigned i=0;i<p.count;i++){
        segment s=p.seg[i];uint8_t *d=ram+s.da-0x10000;
        memcpy(d,bytes+s.off,s.filesz);memset(d+s.filesz,0,s.memsz-s.filesz);
    }
    free(bytes);
    assert(u32(ram+4096)==41&&u32(ram+4100)==0&&ram[4104]==0xcc);
    uc_engine *uc=NULL;
    check_uc(uc_open(UC_ARCH_ARM,UC_MODE_ARM,&uc));
    check_uc(uc_ctl_set_cpu_model(uc,UC_CPU_ARM_CORTEX_A9));
    check_uc(uc_mem_map(uc,0x10000,4096,UC_PROT_READ|UC_PROT_EXEC));
    check_uc(uc_mem_map(uc,0x11000,4096,UC_PROT_READ|UC_PROT_WRITE));
    check_uc(uc_mem_write(uc,0x10000,ram,4096));
    check_uc(uc_mem_write(uc,0x11000,ram+4096,4096));
    check_uc(uc_emu_start(uc,p.entry_da,0x1001c,1000000,7));
    uint8_t result[4];check_uc(uc_mem_read(uc,0x11004,result,4));
    assert(u32(result)==42);
    uint32_t pc=0;check_uc(uc_reg_read(uc,UC_ARM_REG_PC,&pc));assert(pc==0x1001c);
    check_uc(uc_close(uc));
    puts("PASS: toolchain ARM ELF32 executed on Unicorn Cortex-A9 model; result=42");
    return 0;
}
