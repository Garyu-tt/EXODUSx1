#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/resource.h>
#include "tcp_mtls_bio.h"
static mbedtls_ms_time_t simulated_clock=10000;
mbedtls_ms_time_t mbedtls_ms_time(void){return simulated_clock;}
static int fd_count(void){DIR *d=opendir("/proc/self/fd");assert(d);int n=0;while(readdir(d))++n;closedir(d);return n;}
int main(void){
    int before=fd_count();
    ext_pair p={.client=-1,.server=-1};
    assert(ext_open(&p));
    unsigned char b[32768],out[32768];memset(b,0xA5,sizeof(b));
    assert(ext_recv(&p.server,out,sizeof(out))==MBEDTLS_ERR_SSL_WANT_READ);
    int n=ext_send(&p.client,b,sizeof(b));assert(n>0&&n<=16384);
    int got=0;for(int i=0;i<200&&got<n;++i){ext_wait(&p);int r=ext_recv(&p.server,out+got,sizeof(out)-got);if(r>0)got+=r;else assert(r==MBEDTLS_ERR_SSL_WANT_READ);}
    assert(got==n&&!memcmp(b,out,n));
    puts("PASS: empty read and 16 KiB send bound");
    assert(shutdown(p.client,SHUT_WR)==0);
    int eof=0;for(int i=0;i<200;++i){ext_wait(&p);int r=ext_recv(&p.server,out,sizeof(out));if(!r){eof=1;break;}assert(r==MBEDTLS_ERR_SSL_WANT_READ);}
    assert(eof);ext_close(&p);ext_close(&p);assert(fd_count()==before);
    puts("PASS: peer EOF, idempotent cleanup");
    assert(ext_within(10000));simulated_clock=14999;assert(ext_within(10000));simulated_clock=15000;assert(!ext_within(10000));simulated_clock=9999;assert(!ext_within(10000));
    puts("PASS: exact deadline boundary and clock rollback");
    struct rlimit old,lim;assert(getrlimit(RLIMIT_NOFILE,&old)==0);lim=old;lim.rlim_cur=4;assert(setrlimit(RLIMIT_NOFILE,&lim)==0);
    assert(!ext_open(&p));assert(p.client==-1&&p.server==-1);assert(setrlimit(RLIMIT_NOFILE,&old)==0);assert(fd_count()==before);
    puts("PASS: descriptor exhaustion fails closed without leak");
    return 0;
}
