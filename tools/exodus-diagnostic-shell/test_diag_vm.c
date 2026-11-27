#include "diag_vm.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    TdaVm v={0}; const char *err;
    assert(!tda_vm_run("0x1 0 0o377 0b1 1 2 2 0 1 6 63 0 7 7 63 0",&v));
    assert(v.reg[0]==257&&v.reg[7]==257&&v.mem[63]==257&&v.steps==5);
    assert(!tda_vm_run("1 0 18446744073709551615 1 1 1 2 0 1 0",&v)&&v.reg[0]==0);
    assert(!tda_vm_run("1 0 3 1 1 5 4 0 1 3 0 1 8 0 1 9 0 1 5 0 2 10 0 2 11 0 1 0",&v)&&v.reg[0]==1);
    const char *bad[]={"","12","1 8 0 0","6 64 0 0","7 0 64 0","5 0 64 0","1 0 -1 0","1 0 0o8 0","1 0 18446744073709551616 0","0 1","1 0","1 0 0x 0"};
    TdaVm old=v;
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++){err=tda_vm_run(bad[i],&v);assert(err&&memcmp(&v,&old,sizeof v)==0);}
    char longrun[1021]={0};
    for(unsigned i=0;i<64;i++)strcat(longrun,"1 0 1 ");
    strcat(longrun,"0");assert(!strcmp(tda_vm_run(longrun,&v),"step_limit"));
    puts("VM mixed radix, arithmetic, bounds, overflow, termination and transactional failure PASS");
}
