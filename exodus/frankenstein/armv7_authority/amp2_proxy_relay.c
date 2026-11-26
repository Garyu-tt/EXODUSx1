/* Native FreeBSD ARMv7 console transport only. All AMP replies originate
 * from the kernel module, not this process. No codec or synthetic authority. */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
enum { FRAME=64, COUNT=16, LANES=8 };
static pid_t children[LANES];
static int go[LANES];
static uint64_t get64(const uint8_t *p){
    uint64_t v=0;for(unsigned i=8;i;i--)v=(v<<8)|p[i-1];return v;
}
static int snapshot(uint32_t out[LANES*4]){
    size_t n=LANES*4*sizeof(uint32_t);
    return sysctlbyname("kern.exodus_amp2_snapshot",out,&n,NULL,0)==0 &&
        n==LANES*4*sizeof(uint32_t);
}
static int start_workers(void){
    for(unsigned i=0;i<LANES;i++){
        int rd[2],wr[2];if(pipe(rd)||pipe(wr))return 0;
        pid_t p=fork();if(p<0)return 0;
        if(p==0){
            close(rd[0]);close(wr[1]);
            for(unsigned j=0;j<i;j++)close(go[j]);
            uint32_t slot=i,pid=0;size_t n=sizeof(pid);uint8_t code;
            if(sysctlbyname("kern.exodus_amp2_bind",&pid,&n,&slot,sizeof(slot))||
               pid!=(uint32_t)getpid()||n!=sizeof(pid))_exit(101);
            if(write(rd[1],"R",1)!=1||read(wr[0],&code,1)!=1)_exit(102);
            _exit(code);
        }
        children[i]=p;go[i]=wr[1];close(rd[1]);close(wr[0]);
        char ready=0;int ok=read(rd[0],&ready,1)==1&&ready=='R';close(rd[0]);
        if(!ok)return 0;
    }
    uint32_t v[LANES*4];if(!snapshot(v))return 0;
    for(unsigned i=0;i<LANES;i++)if(v[4*i]!=(uint32_t)children[i]||v[4*i+1])return 0;
    return 1;
}
static void cleanup(void){
    for(unsigned i=0;i<LANES;i++)if(children[i]>0){
        if(go[i]>=0)close(go[i]);
        kill(children[i],SIGTERM);
        while(waitpid(children[i],NULL,0)<0&&errno==EINTR){}
        children[i]=0;go[i]=-1;
    }
}
static int nibble(char c){
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    return -1;
}
int main(void){
    uint8_t q[FRAME],r[FRAME];char line[2*FRAME+2];
    int pid=0;size_t len=sizeof(pid);
    if(sizeof(void*)!=4||sysctlbyname("kern.exodus_amp2_caller_pid",&pid,&len,NULL,0)||
       len!=sizeof(pid)||pid!=getpid()||pid<=0)return 1;
    unsigned count=0;len=sizeof(count);
    if(sysctlbyname("kern.exodus_amp2_count",&count,&len,NULL,0)||count!=0)return 2;
    if(atexit(cleanup)||!start_workers())return 9;
    printf("EXODUS_LIVE_READY pid=%d\n",pid);fflush(stdout);
    for(unsigned n=0;n<COUNT;n++){
        if(!fgets(line,sizeof line,stdin)||strlen(line)!=2*FRAME+1||line[2*FRAME]!='\n')return 3;
        for(unsigned i=0;i<FRAME;i++){
            int a=nibble(line[2*i]),b=nibble(line[2*i+1]);
            if(a<0||b<0)return 4;
            q[i]=(uint8_t)((a<<4)|b);
        }
        len=sizeof(r);
        if(sysctlbyname("kern.exodus_amp2",r,&len,q,sizeof q)||len!=sizeof r)return 5;
        const uint64_t base=UINT64_C(0x1234567887654300);
        uint64_t task=get64(q+24);
        if(task<=base||task>base+LANES)return 10;
        unsigned i=(unsigned)(task-base-1);
        if(children[i]<=0)return 11;
        if(q[6]==1){
            if(get64(r+40)!=(uint64_t)children[i])return 12;
        }else if(q[6]==2){
            uint8_t code=(uint8_t)get64(q+32);int status;
            uint8_t final[FRAME];size_t final_len=sizeof(final);
            /* Prove the kernel refuses final acknowledgement before exit. */
            if(sysctlbyname("kern.exodus_amp2_commit_exit",final,&final_len,q,sizeof q)!=-1||errno!=EAGAIN)return 18;
            if(write(go[i],&code,1)!=1)return 13;
            close(go[i]);go[i]=-1;
            pid_t p=children[i],got;
            do{got=waitpid(p,&status,0);}while(got<0&&errno==EINTR);
            if(got!=p)return 14;
            children[i]=0;
            if(!WIFEXITED(status)||WEXITSTATUS(status)!=code)return 15;
            uint32_t v[LANES*4];
            if(!snapshot(v)||v[4*i]!=(uint32_t)p||v[4*i+1]!=1||
               v[4*i+2]!=code||v[4*i+3]!=0)return 16;
            /* Wrong request must not consume the real confirmation. */
            uint8_t wrong[FRAME];memcpy(wrong,q,sizeof wrong);wrong[32]^=1;
            final_len=sizeof(final);
            if(sysctlbyname("kern.exodus_amp2_commit_exit",final,&final_len,wrong,sizeof wrong)!=-1||errno!=EINVAL)return 19;
            final_len=sizeof(final);
            if(sysctlbyname("kern.exodus_amp2_commit_exit",final,&final_len,q,sizeof q)||
               final_len!=sizeof(final)||memcmp(final,r,sizeof r))return 20;
            memcpy(r,final,sizeof r);
            final_len=sizeof(final);
            if(sysctlbyname("kern.exodus_amp2_commit_exit",final,&final_len,q,sizeof q)!=-1||errno!=EINVAL)return 21;
        }else return 17;
        /* Exit replies must additionally pass the kernel's one-shot commit gate. */
        fputs("EXODUS_LIVE_REPLY ",stdout);
        for(unsigned i=0;i<FRAME;i++)printf("%02x",(unsigned)r[i]);
        putchar('\n');if(fflush(stdout))return 6;
    }
    len=sizeof(count);
    if(sysctlbyname("kern.exodus_amp2_count",&count,&len,NULL,0)||count!=COUNT)return 7;
    unsigned exited=0;len=sizeof(exited);
    if(sysctlbyname("kern.exodus_amp2_exited23",&exited,&len,NULL,0)||exited!=8)return 8;
    puts("EXODUS_AMP2_KERNEL_EXITED23=8");
    puts("EXODUS_NATIVE_PROXY_ACK_PASS children=8 waitpid=8");
    puts("EXODUS_LIVE_KERNEL_COUNT=16");
    return fflush(stdout)!=0;
}
