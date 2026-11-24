#include "nxbsd_commands.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    struct nxbsd_context p;
    int ends[2], status[3]={-1,-1,-1};
    char b[64]={0};
    assert(nxbsd_context_init(&p,"sdmc:/")==0);
    assert(nxbsd_fd_pipe_from(&p.fd,3,ends)==0);
    assert(nxbsd_fd_dup2(&p.fd,ends[1],1)==1);
    assert(nxbsd_fd_close(&p.fd,ends[1])==0);
    assert(nxbsd_context_setenv(&p,"NAME","NX-BSD")==0);
    const char *echo[]={"echo","hello","world"};
    const char *cat[]={"cat","-"};
    const char *wc[]={"wc","-c"};
    const char *missing[]={"not-installed"};
    const struct nxbsd_invocation calls[]={{3,echo},{2,cat},{2,wc}};
    const struct nxbsd_invocation bad[]={{3,echo},{1,missing}};
    assert(nxbsd_commands_pipeline(&p,bad,2,status)==-1 && errno==ENOENT);
    assert(status[0]==-1 && status[1]==-1);
    assert(nxbsd_fd_read(&p.fd,ends[0],b,sizeof(b),1)==-1 && errno==EAGAIN);
    assert(nxbsd_commands_pipeline(&p,calls,3,status)==0);
    assert(status[0]==0 && status[1]==0 && status[2]==0);
    assert(nxbsd_fd_read(&p.fd,ends[0],b,sizeof(b),1)==3 && !memcmp(b,"12\n",3));
    const char *env[]={"printenv","NAME"};
    assert(nxbsd_command_run(&p,2,env)==0);
    assert(nxbsd_fd_read(&p.fd,ends[0],b,sizeof(b),1)==7 && !memcmp(b,"NX-BSD\n",7));
    const char *literal[]={"echo","a; b | c"};
    assert(nxbsd_command_run(&p,2,literal)==0);
    assert(nxbsd_fd_read(&p.fd,ends[0],b,sizeof(b),1)==9 && !memcmp(b,"a; b | c\n",9));
    assert(nxbsd_command_run(&p,1,missing)==127);
    const char *pwd[]={"pwd"};
    assert(nxbsd_command_run(&p,1,pwd)==0);
    assert(nxbsd_fd_read(&p.fd,ends[0],b,sizeof(b),1)==7 && !memcmp(b,"sdmc:/\n",7));
    assert(nxbsd_fd_close(&p.fd,1)==0);
    assert(nxbsd_fd_read(&p.fd,ends[0],b,1,0)==0);
    nxbsd_context_destroy(&p);
    puts("NX-BSD command registry contracts passed");
    return 0;
}
