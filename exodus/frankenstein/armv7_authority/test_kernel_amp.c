#include "amp_execution_contract.c"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
int main(void){
    struct amp_session s[8];
    uint8_t q[AMP_SIZE],r[AMP_SIZE];
    uint64_t result;
    const uint64_t gen=UINT64_C(0x1020304050607080),base=UINT64_C(0x1234567887654300);
    int pid=0;size_t n=sizeof(pid);
    assert(sizeof(void*)==4);
    assert(!sysctlbyname("kern.exodus_amp1_caller_pid",&pid,&n,NULL,0));
    assert(n==sizeof(pid)&&pid==getpid()&&pid>0);
    printf("EXODUS_ARMV7_NATIVE_CALLER_PID_PASS pid=%d\n",pid);
    for(unsigned i=0;i<8;i++){assert(init(&s[i],gen,base+i+1));s[i].last=UINT64_C(0xffffffff);}
    for(unsigned op=1;op<=2;op++)for(unsigned i=0;i<8;i++){
        assert(request(&s[i],op,op==2?23:0,q));
        n=sizeof(r);
        if(op==1&&i==0){
            assert(sysctlbyname("kern.exodus_amp1",r,&n,q,63)==-1&&errno==EINVAL);
            n=sizeof(r);
        }
        assert(!sysctlbyname("kern.exodus_amp1",r,&n,q,sizeof(q)));
        assert(n==AMP_SIZE&&complete(&s[i],r,n,&result));
        assert(result==(op==1?base+i+1:0));
        assert(s[i].closed==(op==2));
    }
    unsigned count=0;n=sizeof(count);
    assert(!sysctlbyname("kern.exodus_amp1_count",&count,&n,NULL,0)&&count==16);
    n=sizeof(r);
    assert(sysctlbyname("kern.exodus_amp1",r,&n,q,sizeof(q))==-1&&errno==EINVAL);
    count=0;n=sizeof(count);
    assert(!sysctlbyname("kern.exodus_amp1_count",&count,&n,NULL,0)&&count==16);
    puts("EXODUS_ARMV7_KERNEL_AMP_PASS lanes=8 transactions=16 replay=reject");
    return 0;
}
