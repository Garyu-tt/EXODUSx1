/* Separate ARM32 synthetic authority; no FreeBSD claim. */
#include "amp_execution_contract.c"
#include <stdio.h>
int main(void){
    struct amp_session s[8];uint8_t q[AMP_SIZE],r[AMP_SIZE];struct amp_msg m;
    const uint64_t gen=UINT64_C(0x1020304050607080),base=UINT64_C(0x1234567887654300);
    for(unsigned i=0;i<8;i++){if(!init(&s[i],gen,base+i+1))return 1;s[i].last=UINT64_C(0xffffffff);}
    for(unsigned n=0;n<16;n++){
        if(fread(q,1,AMP_SIZE,stdin)!=AMP_SIZE||!decode(&m,q,AMP_SIZE)||m.task<=base||m.task>base+8)return 2;
        struct amp_session *p=&s[m.task-base-1];
        if(!serve(p,q,sizeof q,r)||fwrite(r,1,AMP_SIZE,stdout)!=AMP_SIZE||fflush(stdout))return 3;
    }
    for(unsigned i=0;i<8;i++)if(!s[i].closed||s[i].last!=UINT64_C(0x100000001))return 4;
    fprintf(stderr,"AMP_SMP_AUTHORITY_PASS width=%zu lanes=8\n",sizeof(void*)*8);
    return 0;
}
