#include "amp_wait_contract.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#define N 8
static uint64_t seq=UINT64_C(0xffffffff);
static struct ew_frame ask(uint64_t child){
    return (struct ew_frame){.generation=UINT64_C(0x1020304050607080),.seq=seq+1,
        .parent=UINT64_C(0x100000001),.child=child,.kind=EW_REQUEST,.options=EW_NOHANG};
}
static struct ew_frame call(uint64_t child){
    struct ew_frame f=ask(child),out;uint8_t q[EW_SIZE],r[EW_SIZE];size_t n=sizeof r;
    assert(ew_encode(q,&f));
    assert(!sysctlbyname("kern.exodus_awt_exchange",r,&n,q,sizeof q));
    assert(ew_match(&f,r,n,&out));seq++;
    n=sizeof r;assert(sysctlbyname("kern.exodus_awt_exchange",r,&n,q,sizeof q)==-1&&errno==EINVAL);
    return out;
}
int main(void){
    pid_t p[N];int go[N];uint32_t token=1,pid=0;size_t n=4;
    assert(sizeof(void*)==4);
    assert(!sysctlbyname("kern.exodus_awt_parent",&pid,&n,&token,4)&&pid==(uint32_t)getpid());
    for(unsigned i=0;i<N;i++){
        int rd[2],wr[2];assert(!pipe(rd)&&!pipe(wr));
        p[i]=fork();assert(p[i]>=0);
        if(!p[i]){
            close(rd[0]);close(wr[1]);for(unsigned j=0;j<i;j++)close(go[j]);
            uint32_t slot=i,id=0;size_t len=4;
            if(sysctlbyname("kern.exodus_awt_child",&id,&len,&slot,4)||id!=(uint32_t)getpid())_exit(101);
            struct ew_frame f=ask(9+i);uint8_t q[EW_SIZE],r[EW_SIZE];len=sizeof r;
            if(!ew_encode(q,&f)||sysctlbyname("kern.exodus_awt_exchange",r,&len,q,sizeof q)!=-1||errno!=EPERM)_exit(102);
            char c='R';if(write(rd[1],&c,1)!=1||read(wr[0],&c,1)!=1)_exit(103);
            _exit(23+(int)i);
        }
        close(rd[1]);close(wr[0]);go[i]=wr[1];char c;
        assert(read(rd[0],&c,1)==1&&c=='R');close(rd[0]);
    }
    for(unsigned i=0;i<N;i++){struct ew_frame f=call(9+i);assert(!f.pid&&!f.error&&!f.wait_status);}
    assert(call(999).error==EW_ECHILD);
    for(unsigned i=0;i<N;i++){
        if(i==N-1)assert(!kill(p[i],SIGTERM));
        else assert(write(go[i],"X",1)==1);
        close(go[i]);int st;assert(waitpid(p[i],&st,0)==p[i]);
        struct ew_frame f=call(9+i);
        assert(!f.error&&f.pid==(uint32_t)p[i]&&f.wait_status==(uint32_t)st);
        assert(call(9+i).error==EW_ECHILD);
    }
    puts("EXODUS_NATIVE_AWT_PASS live=8 exit=8 replay=25 foreign=8");
}
