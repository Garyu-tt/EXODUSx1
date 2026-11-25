#include "nxbsd_context.h"
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void *child(void *arg)
{
    struct nxbsd_context *c=arg;
    assert(nxbsd_fd_close(&c->fd,0)==0);
    assert(nxbsd_context_setenv(c,"X","child")==0);
    assert(nxbsd_context_setcwd(c,"sdmc:/child")==0);
    assert(nxbsd_fd_write(&c->fd,1,"abc",3,0)==3);
    c->status=7;
    nxbsd_context_destroy(c);
    return NULL;
}
int main(void)
{
    struct nxbsd_context parent, other;
    int p[2];
    char b[8];
    pthread_t t;
    assert(nxbsd_context_init(&parent,"sdmc:/")==0);
    assert(nxbsd_context_setenv(&parent,"X","parent")==0);
    assert(nxbsd_context_setenv(&parent,"X=Y","bad")==-1 && errno==EINVAL);
    assert(nxbsd_fd_pipe(&parent.fd,p)==0 && p[0]==0 && p[1]==1);
    assert(nxbsd_context_clone(&other,&parent)==0);
    assert(!strcmp(nxbsd_context_getenv(&other,"X"),"parent"));
    assert(nxbsd_fd_close(&parent.fd,1)==0);
    assert(nxbsd_fd_read(&parent.fd,0,b,1,1)==-1 && errno==EAGAIN);
    assert(pthread_create(&t,NULL,child,&other)==0);
    assert(nxbsd_fd_read(&parent.fd,0,b,3,0)==3 && !memcmp(b,"abc",3));
    assert(nxbsd_fd_read(&parent.fd,0,b,1,0)==0);
    assert(pthread_join(t,NULL)==0);
    assert(!strcmp(nxbsd_context_getenv(&parent,"X"),"parent"));
    assert(!strcmp(parent.cwd,"sdmc:/") && parent.status==0);
    assert(nxbsd_context_unsetenv(&parent,"X")==0);
    assert(nxbsd_context_getenv(&parent,"X")==NULL);
    assert(nxbsd_context_setcwd(&parent,"relative")==-1 && errno==EINVAL);
    nxbsd_context_destroy(&parent);
    puts("NX-BSD context contracts passed");
    return 0;
}
