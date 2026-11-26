/* Native FreeBSD ARMv7 console transport only. All AMP replies originate
 * from the kernel module, not this process. No codec or synthetic authority. */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
enum { FRAME=64, COUNT=16 };
static int nibble(char c){
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    return -1;
}
int main(void){
    uint8_t q[FRAME],r[FRAME];char line[2*FRAME+2];
    int pid=0;size_t len=sizeof(pid);
    if(sizeof(void*)!=4||sysctlbyname("kern.exodus_amp1_caller_pid",&pid,&len,NULL,0)||
       len!=sizeof(pid)||pid!=getpid()||pid<=0)return 1;
    unsigned count=0;len=sizeof(count);
    if(sysctlbyname("kern.exodus_amp1_count",&count,&len,NULL,0)||count!=0)return 2;
    printf("EXODUS_LIVE_READY pid=%d\n",pid);fflush(stdout);
    for(unsigned n=0;n<COUNT;n++){
        if(!fgets(line,sizeof line,stdin)||strlen(line)!=2*FRAME+1||line[2*FRAME]!='\n')return 3;
        for(unsigned i=0;i<FRAME;i++){
            int a=nibble(line[2*i]),b=nibble(line[2*i+1]);
            if(a<0||b<0)return 4;
            q[i]=(uint8_t)((a<<4)|b);
        }
        len=sizeof(r);
        if(sysctlbyname("kern.exodus_amp1",r,&len,q,sizeof q)||len!=sizeof r)return 5;
        fputs("EXODUS_LIVE_REPLY ",stdout);
        for(unsigned i=0;i<FRAME;i++)printf("%02x",(unsigned)r[i]);
        putchar('\n');if(fflush(stdout))return 6;
    }
    len=sizeof(count);
    if(sysctlbyname("kern.exodus_amp1_count",&count,&len,NULL,0)||count!=COUNT)return 7;
    puts("EXODUS_LIVE_KERNEL_COUNT=16");
    return fflush(stdout)!=0;
}
