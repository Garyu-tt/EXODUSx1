/* Native parent, kernel-only AWT reply generation. */
#include "amp_wait_contract.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#define N 8
static pid_t kids[N];
static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static void cleanup(void){
    for(unsigned i=0;i<N;i++)if(kids[i]>0){kill(kids[i],SIGTERM);while(waitpid(kids[i],NULL,0)<0&&errno==EINTR){}}
}
int main(void){
    assert(sizeof(void*)==4);assert(!atexit(cleanup));
    uint32_t token=1,pid=0;size_t len=4;
    assert(!sysctlbyname("kern.exodus_awt_parent",&pid,&len,&token,4)&&pid==(uint32_t)getpid());
    pid_t pids[N];int statuses[N];unsigned seen=0;
    const uint64_t base=UINT64_C(0x1234567887654300);
    /* Native children exit before A57 starts; kernel exit hook retains records. */
    for(unsigned i=0;i<N;i++){
        pid_t p=fork();assert(p>=0);
        if(!p){
            uint32_t slot=i,id=0;size_t n=4;
            if(sysctlbyname("kern.exodus_awt_child",&id,&n,&slot,4)||id!=(uint32_t)getpid())_exit(101);
            _exit(23+(int)i);
        }
        kids[i]=p;pids[i]=p;int st;
        assert(waitpid(p,&st,0)==p);kids[i]=0;
        assert(WIFEXITED(st)&&WEXITSTATUS(st)==23+(int)i);
        statuses[i]=st;
    }
    printf("EXODUS_LIVE_READY pid=%d\n",getpid());fflush(stdout);
    for(unsigned j=0;j<N;j++){
        char line[130];uint8_t q[EW_SIZE],r[EW_SIZE];struct ew_frame f,out;
        assert(fgets(line,sizeof line,stdin)&&strlen(line)==129&&line[128]=='\n');
        for(unsigned i=0;i<EW_SIZE;i++){int h=nibble(line[i*2]),l=nibble(line[i*2+1]);assert(h>=0&&l>=0);q[i]=(uint8_t)((h<<4)|l);}
        assert(ew_decode(&f,q,sizeof q)&&f.parent>base&&f.parent<=base+N);
        unsigned i=(unsigned)(f.parent-base-1);
        assert(!(seen&(1u<<i))&&f.child==9+i);
        len=sizeof r;assert(!sysctlbyname("kern.exodus_awt_exchange",r,&len,q,sizeof q));
        assert(ew_match(&f,r,len,&out)&&!out.error&&out.pid==(uint32_t)pids[i]&&out.wait_status==(uint32_t)statuses[i]);
        seen|=1u<<i;
        uint8_t bad[EW_SIZE];size_t n=sizeof bad;
        assert(sysctlbyname("kern.exodus_awt_exchange",bad,&n,q,sizeof q)==-1&&errno==EINVAL);
        fputs("EXODUS_LIVE_REPLY ",stdout);
        for(unsigned b=0;b<EW_SIZE;b++)printf("%02x",(unsigned)r[b]);
        putchar('\n');fflush(stdout);
    }
    assert(seen==255);
    puts("EXODUS_LIVE_AWT_NATIVE_PASS children=8 replay=8");return fflush(stdout)!=0;
}
