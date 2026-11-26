#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#define SLOTS 8
static void snap(uint32_t v[SLOTS*4]){
    size_t n=SLOTS*4*sizeof(uint32_t);
    assert(!sysctlbyname("kern.exodus_proxy_snapshot",v,&n,NULL,0));
    assert(n==SLOTS*4*sizeof(uint32_t));
}
int main(void){
    pid_t p[SLOTS];int ready[SLOTS][2],go[SLOTS][2];
    uint32_t v[SLOTS*4];assert(sizeof(void*)==4);
    for(unsigned i=0;i<SLOTS;i++){
        assert(!pipe(ready[i])&&!pipe(go[i]));
        p[i]=fork();assert(p[i]>=0);
        if(!p[i]){
            close(ready[i][0]);close(go[i][1]);
            uint32_t pid=0,slot=i;size_t n=sizeof(pid);char c='R';
            if(sysctlbyname("kern.exodus_proxy_bind",&pid,&n,&slot,sizeof(slot))||
               pid!=(uint32_t)getpid()||n!=sizeof(pid))_exit(101);
            n=sizeof(pid);
            if(sysctlbyname("kern.exodus_proxy_bind",&pid,&n,&slot,sizeof(slot))!=-1||
               errno!=EBUSY)_exit(102);
            if(write(ready[i][1],&c,1)!=1||read(go[i][0],&c,1)!=1)_exit(103);
            _exit(23+i);
        }
        close(ready[i][1]);close(go[i][0]);char c;
        assert(read(ready[i][0],&c,1)==1&&c=='R');close(ready[i][0]);
    }
    snap(v);
    for(unsigned i=0;i<SLOTS;i++){
        assert(v[4*i]==(uint32_t)p[i]&&v[4*i+1]==1);
        for(unsigned j=0;j<i;j++)assert(p[i]!=p[j]);
        int st;assert(waitpid(p[i],&st,WNOHANG)==0);
    }
    /* Normal exit and real native signal; only own child processes. */
    for(unsigned i=0;i<SLOTS;i++){
        if(i==SLOTS-1)assert(!kill(p[i],SIGTERM));
        else assert(write(go[i][1],"X",1)==1);
        close(go[i][1]);
        int st;assert(waitpid(p[i],&st,0)==p[i]);
        if(i==SLOTS-1)assert(WIFSIGNALED(st)&&WTERMSIG(st)==SIGTERM);
        else assert(WIFEXITED(st)&&WEXITSTATUS(st)==23+(int)i);
    }
    snap(v);
    for(unsigned i=0;i<SLOTS;i++){
        assert(v[4*i]==(uint32_t)p[i]&&v[4*i+1]==2);
        if(i==SLOTS-1)assert(v[4*i+3]==SIGTERM);
        else assert(v[4*i+2]==23+i&&v[4*i+3]==0);
    }
    puts("EXODUS_NATIVE_PROXY_PASS children=8 normal=7 signal=1 waitpid=8");
    return 0;
}
