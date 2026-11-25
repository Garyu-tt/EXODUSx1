#include "nxbsd_task.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int child(struct nxbsd_context *c, void *arg)
{
    (void)arg;
    char b;
    assert(nxbsd_fd_close(&c->fd,1)==0);
    assert(nxbsd_fd_read(&c->fd,0,&b,1,0)==1 && b=='x');
    assert(nxbsd_context_setenv(c,"X","child")==0);
    return 23;
}
static int immediate(struct nxbsd_context *c, void *arg)
{
    (void)c; (void)arg;
    return 7;
}
int main(void)
{
    struct nxbsd_context parent;
    struct nxbsd_scheduler s;
    struct nxbsd_task_id id, ids[NXBSD_TASK_MAX], unused;
    int p[2], status=-1, exited=-1;
    assert(nxbsd_scheduler_init(&s)==0);
    assert(nxbsd_context_init(&parent,"sdmc:/")==0);
    assert(nxbsd_context_setenv(&parent,"X","parent")==0);
    assert(nxbsd_fd_pipe(&parent.fd,p)==0);
    assert(nxbsd_task_spawn(&s,&parent,child,NULL,&id)==0);
    assert(nxbsd_task_poll(&s,id,&exited)==0 && !exited);
    assert(nxbsd_task_wait(&s,id,&status,1)==-1 && errno==EAGAIN);
    assert(nxbsd_scheduler_destroy(&s)==-1 && errno==EBUSY);
    assert(nxbsd_fd_write(&parent.fd,p[1],"x",1,0)==1);
    assert(nxbsd_task_wait(&s,id,&status,0)==0 && status==23);
    assert(nxbsd_task_wait(&s,id,&status,0)==-1 && errno==ECHILD);
    assert(!strcmp(nxbsd_context_getenv(&parent,"X"),"parent"));
    nxbsd_fd_clear(&parent.fd);
    for(unsigned i=0;i<NXBSD_TASK_MAX;i++)
        assert(nxbsd_task_spawn(&s,&parent,immediate,NULL,&ids[i])==0);
    assert(ids[0].slot==id.slot && ids[0].generation!=id.generation);
    assert(nxbsd_task_poll(&s,id,&exited)==-1 && errno==ECHILD);
    assert(nxbsd_task_spawn(&s,&parent,immediate,NULL,&unused)==-1 && errno==EAGAIN);
    for(unsigned i=0;i<NXBSD_TASK_MAX;i++)
        assert(nxbsd_task_wait(&s,ids[i],&status,0)==0 && status==7);
    nxbsd_context_destroy(&parent);
    assert(nxbsd_scheduler_destroy(&s)==0);
    puts("NX-BSD task scheduler contracts passed");
    return 0;
}
