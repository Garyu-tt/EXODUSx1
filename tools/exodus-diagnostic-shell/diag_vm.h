/* SPDX-License-Identifier: MIT
 * Bounded diagnostic VM, not native executable code. */
#ifndef TDA_VM_H
#define TDA_VM_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define TDA_VM_WORDS 64
#define TDA_VM_REGS 8
#define TDA_VM_MAX_STEPS 64
typedef struct { uint64_t reg[8], mem[64]; unsigned steps; } TdaVm;
/* Unsigned integers: decimal, 0x hex, 0o octal, 0b binary. */
static int tda_num(const char **s,uint64_t *out){
    const char *p=*s;while(*p==' ')p++;
    unsigned base=10,digits=0;uint64_t v=0;
    if(p[0]=='0'&&p[1]){
        if(p[1]=='x'){base=16;p+=2;}
        else if(p[1]=='o'){base=8;p+=2;}
        else if(p[1]=='b'){base=2;p+=2;}
    }
    while(*p&&*p!=' '){
        unsigned d;
        if(*p>='0'&&*p<='9')d=(unsigned)(*p-'0');
        else if(*p>='a'&&*p<='f')d=(unsigned)(*p-'a'+10);
        else if(*p>='A'&&*p<='F')d=(unsigned)(*p-'A'+10);
        else return 0;
        if(d>=base||v>(UINT64_MAX-d)/base||++digits>64)return 0;
        v=v*base+d;p++;
    }
    if(!digits)return 0;
    *s=p;*out=v;return 1;
}
/* 0 END, 1 SET r imm, 2 ADD r r, 3 XOR r r, 4 MUL r r,
 * 5 SHL r imm, 6 STORE slot r, 7 LOAD r slot, 8 AND r r,
 * 9 OR r r, 10 SHR r imm, 11 EQ r r (0/1).
 * Transactional: publish no result until valid END. */
static const char *tda_vm_run(const char *p,TdaVm *out){
    TdaVm v={0};uint64_t op,a,b;
    if(strlen(p)>1020)return "length";
    for(unsigned i=0;i<TDA_VM_MAX_STEPS;i++){
        if(!tda_num(&p,&op))return "opcode";
        if(op==0){
            while(*p==' ')p++;
            if(*p)return "trailing";
            *out=v;return NULL;
        }
        if(op>11)return "opcode";
        if(!tda_num(&p,&a)||!tda_num(&p,&b))return "operand";
        if(op==6){
            if(a>=64||b>=8)return "bounds";
            v.mem[a]=v.reg[b];
        }else{
            if(a>=8)return "bounds";
            if(op==1)v.reg[a]=b;
            else if(op==5||op==10){
                if(b>=64)return "shift";
                if(op==5)v.reg[a]<<=b;else v.reg[a]>>=b;
            }else if(op==7){
                if(b>=64)return "bounds";
                v.reg[a]=v.mem[b];
            }else{
                if(b>=8)return "bounds";
                switch(op){
                    case 2:v.reg[a]+=v.reg[b];break;
                    case 3:v.reg[a]^=v.reg[b];break;
                    case 4:v.reg[a]*=v.reg[b];break;
                    case 8:v.reg[a]&=v.reg[b];break;
                    case 9:v.reg[a]|=v.reg[b];break;
                    case 11:v.reg[a]=(v.reg[a]==v.reg[b]);break;
                    default:return "opcode";
                }
            }
        }
        v.steps++;
    }
    return "step_limit";
}
#endif
