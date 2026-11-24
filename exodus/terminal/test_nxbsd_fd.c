#include "nxbsd_fd.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int main(void)
{
    struct nxbsd_fd_table t;
    nxbsd_fd_init(&t);
    int p[2];
    char b[8] = {0};
    assert(nxbsd_fd_pipe(&t, p) == 0);
    assert(p[0] == 0 && p[1] == 1);
    assert(nxbsd_fd_dup2(&t,p[1],5) == 5);
    assert(nxbsd_fd_close(&t,p[1]) == 0);
    assert(nxbsd_fd_read(&t,p[0],b,1,1) == -1 && errno == EAGAIN);
    assert(nxbsd_fd_write(&t,5,"abc",3,0) == 3);
    assert(nxbsd_fd_read(&t,p[0],b,sizeof(b),0) == 3);
    assert(!memcmp(b,"abc",3));
    assert(nxbsd_fd_write(&t,p[0],"x",1,0) == -1 && errno == EBADF);
    assert(nxbsd_fd_dup2(&t,5,5) == 5);
    assert(nxbsd_fd_dup2(&t,5,p[0]) == p[0]);
    assert(nxbsd_fd_write(&t,5,"x",1,0) == -1 && errno == EPIPE);
    nxbsd_fd_clear(&t);
    assert(nxbsd_fd_read(&t,0,b,1,0) == -1 && errno == EBADF);
    assert(nxbsd_fd_pipe(&t,p) == 0);
    for(int i=2;i<NXBSD_FD_MAX;i++) assert(nxbsd_fd_dup2(&t,p[1],i)==i);
    int sentinel[2]={-10,-20};
    assert(nxbsd_fd_pipe(&t,sentinel)==-1 && errno==EMFILE);
    assert(sentinel[0]==-10 && sentinel[1]==-20);
    for(int i=1;i<NXBSD_FD_MAX;i++) assert(nxbsd_fd_close(&t,i)==0);
    assert(nxbsd_fd_read(&t,p[0],b,1,0)==0);
    nxbsd_fd_clear(&t);
#ifndef __SWITCH__
    char path[]="/tmp/nxbsd-fd-XXXXXX";
    int f=mkstemp(path);
    assert(f>=0);
    assert(unlink(path)==0);
    int v=nxbsd_fd_adopt(&t,f);
    assert(v==0);
    assert(nxbsd_fd_adopt(&t,f)==-1 && errno==EEXIST);
    assert(nxbsd_fd_dup2(&t,v,1)==1);
    assert(nxbsd_fd_write(&t,v,"ab",2,0)==2);
    assert(nxbsd_fd_close(&t,v)==0);
    assert(nxbsd_fd_write(&t,1,"c",1,0)==1);
    assert(lseek(f,0,SEEK_SET)==0);
    assert(nxbsd_fd_read(&t,1,b,3,0)==3 && !memcmp(b,"abc",3));
    assert(nxbsd_fd_close(&t,1)==0);
    assert(fcntl(f,F_GETFD)==-1 && errno==EBADF);
#endif
    puts("NX-BSD descriptor contracts passed");
    return 0;
}
