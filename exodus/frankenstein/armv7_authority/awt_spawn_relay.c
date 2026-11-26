/* Native parent, kernel-only AWT reply generation. */
#include "amp_spawn_contract.h"
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
static int go[N];
static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static void cleanup(void){
    for(unsigned i=0;i<N;i++)if(kids[i]>0){kill(kids[i],SIGTERM);while(waitpid(kids[i],NULL,0)<0&&errno==EINTR){}}
}
int main(void){
    assert(sizeof(void*)==4);assert(!atexit(cleanup));
    uint32_t token=1,pid=0;size_t len=4;
    assert(!sysctlbyname("kern.exodus_awt_parent",&pid,&len,&token,4)&&pid==(uint32_t)getpid());
    pid_t pids[N];int statuses[N]={0};unsigned phase[N]={0};
    const uint64_t base=UINT64_C(0x1234567887654300);
    printf("EXODUS_LIVE_READY pid=%d\n",getpid());fflush(stdout);
    for(unsigned j=0;j<4*N;j++){
        char line[130];uint8_t q[EW_SIZE],r[EW_SIZE];struct ew_frame f,out;
        assert(fgets(line,sizeof line,stdin)&&strlen(line)==129&&line[128]=='\n');
        for(unsigned i=0;i<EW_SIZE;i++){int h=nibble(line[i*2]),l=nibble(line[i*2+1]);assert(h>=0&&l>=0);q[i]=(uint8_t)((h<<4)|l);}
        int spawn=!memcmp(q,"AWS1",4);
        assert((spawn?aws_decode(&f,q,sizeof q):ew_decode(&f,q,sizeof q))&&f.parent>base&&f.parent<=base+N);
        unsigned i=(unsigned)(f.parent-base-1);
        if(spawn){
            assert(!phase[i]&&!kids[i]&&f.child==9+i&&f.seq==1);
            len=sizeof r;assert(!sysctlbyname("kern.exodus_aws_reserve",r,&len,q,sizeof q)&&len==sizeof r);
            for(unsigned b=0;b<EW_SIZE;b++)assert(!r[b]);
            len=sizeof r;assert(sysctlbyname("kern.exodus_aws_reserve",r,&len,q,sizeof q)==-1&&errno==EBUSY);
            len=sizeof r;assert(sysctlbyname("kern.exodus_aws_commit",r,&len,q,sizeof q)==-1&&errno==EBUSY);
            int rd[2],wr[2];assert(!pipe(rd)&&!pipe(wr));
            pid_t child=fork();assert(child>=0);
            if(!child){
                close(rd[0]);close(wr[1]);
                for(unsigned j=0;j<N;j++)if(kids[j]>0)close(go[j]);
                uint32_t slot=i,id=0;size_t n=4;char c='R';
                if(sysctlbyname("kern.exodus_awt_child",&id,&n,&slot,4)||id!=(uint32_t)getpid())_exit(101);
                if(write(rd[1],&c,1)!=1||read(wr[0],&c,1)!=1)_exit(102);
                _exit(23+(int)i);
            }
            kids[i]=child;pids[i]=child;go[i]=wr[1];close(rd[1]);close(wr[0]);char c;
            assert(read(rd[0],&c,1)==1&&c=='R');close(rd[0]);
            len=sizeof r;assert(!sysctlbyname("kern.exodus_aws_commit",r,&len,q,sizeof q));
            assert(aws_match(&f,r,len,&out)&&out.pid==(uint32_t)child);
            uint8_t bad[EW_SIZE];len=sizeof bad;
            assert(sysctlbyname("kern.exodus_aws_commit",bad,&len,q,sizeof q)==-1&&errno==EINVAL);
            phase[i]=1;
            goto reply;
        }
        assert(phase[i]>=1&&phase[i]<4&&f.child==9+i&&f.seq==UINT64_C(0x100000000)+phase[i]-1);
        len=sizeof r;assert(!sysctlbyname("kern.exodus_awt_exchange",r,&len,q,sizeof q));
        assert(ew_match(&f,r,len,&out));
        if(phase[i]==1){
            int st;assert(!out.error&&!out.pid&&!out.wait_status);
            assert(waitpid(kids[i],&st,WNOHANG)==0);
            /* Preserve the kernel's zero reply. Release only after it was produced. */
            assert(write(go[i],"X",1)==1);close(go[i]);
            assert(waitpid(kids[i],&st,0)==kids[i]);kids[i]=0;
            assert(WIFEXITED(st)&&WEXITSTATUS(st)==23+(int)i);statuses[i]=st;
        }else if(phase[i]==2){
            assert(!out.error&&out.pid==(uint32_t)pids[i]&&out.wait_status==(uint32_t)statuses[i]);
        }else assert(out.error==EW_ECHILD&&!out.pid&&!out.wait_status);
        phase[i]++;
        uint8_t bad[EW_SIZE];size_t n=sizeof bad;
        assert(sysctlbyname("kern.exodus_awt_exchange",bad,&n,q,sizeof q)==-1&&errno==EINVAL);
reply:
        fputs("EXODUS_LIVE_REPLY ",stdout);
        for(unsigned b=0;b<EW_SIZE;b++)printf("%02x",(unsigned)r[b]);
        putchar('\n');fflush(stdout);
    }
    for(unsigned i=0;i<N;i++)assert(phase[i]==4);
    puts("EXODUS_LIVE_AWT_SPAWN_PASS spawn=8 live=8 exit=8 echild=8");return fflush(stdout)!=0;
}
