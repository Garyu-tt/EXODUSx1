/* Synthetic ARM32 peer for A57 syscall/copyout admission only. */
#include "amp_wait_contract.h"
#include <stdio.h>
#include <assert.h>
int main(void){
    struct ew_parent parents[8];struct ew_child c[8]={{0}};
    const uint64_t gen=UINT64_C(0x1020304050607080),base=UINT64_C(0x1234567887654300);
    for(unsigned i=0;i<8;i++){
        parents[i]=(struct ew_parent){gen,base+i+1,UINT64_C(0xffffffff)};
        assert(ew_register(&c[i],9+i,base+i+1,2001+i));
        assert(ew_exit(&c[i],(23+i)<<8));
    }
    uint8_t q[EW_SIZE],r[EW_SIZE];struct ew_frame f;
    for(unsigned n=0;n<8;n++){
        assert(fread(q,1,sizeof q,stdin)==sizeof q&&ew_decode(&f,q,sizeof q));
        assert(f.parent>base&&f.parent<=base+8);
        unsigned i=(unsigned)(f.parent-base-1);
        assert(ew_serve(&parents[i],c,8,q,sizeof q,r));
        assert(fwrite(r,1,sizeof r,stdout)==sizeof r&&!fflush(stdout));
    }
    for(unsigned i=0;i<8;i++)assert(c[i].state==EW_REAPED);
    fputs("AWT_SVC_PEER_PASS width=32 lanes=8\n",stderr);
    return 0;
}
