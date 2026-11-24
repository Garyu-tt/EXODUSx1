#include "nxbsd_pipeline.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#define TOTAL 19001
static int produce(struct nxbsd_context *c,void *arg)
{
    (void)arg;
    unsigned char b[257];
    size_t done=0;
    while(done<TOTAL) {
        size_t n=TOTAL-done;
        if(n>sizeof(b)) n=sizeof(b);
        for(size_t i=0;i<n;i++) b[i]=(unsigned char)((done+i)%251);
        ssize_t w=nxbsd_fd_write(&c->fd,1,b,n,0);
        if(w<0) return errno==EPIPE?17:1;
        if(!w) return 2;
        done+=(size_t)w;
    }
    return 3;
}
static int transform(struct nxbsd_context *c,void *arg)
{
    (void)arg;
    unsigned char b[333];
    for(;;) {
        ssize_t n=nxbsd_fd_read(&c->fd,0,b,sizeof(b),0);
        if(n<0) return 1;
        if(!n) return 4;
        for(ssize_t i=0;i<n;i++) b[i]^=0xa5;
        if(nxbsd_fd_write(&c->fd,1,b,(size_t)n,0)!=n) return 2;
    }
}
static int consume(struct nxbsd_context *c,void *arg)
{
    size_t *total=arg;
    unsigned char b[101];
    for(;;) {
        ssize_t n=nxbsd_fd_read(&c->fd,0,b,sizeof(b),0);
        if(n<0) return 1;
        if(!n) return 5;
        for(ssize_t i=0;i<n;i++)
            assert(b[i]==(unsigned char)(((*total+(size_t)i)%251)^0xa5));
        *total+=(size_t)n;
    }
}
static int early(struct nxbsd_context *c,void *arg)
{
    (void)c; (void)arg; return 9;
}
static int count(struct nxbsd_context *c,void *arg)
{
    (void)c; atomic_fetch_add((atomic_int *)arg,1); return 0;
}
int main(void)
{
    struct nxbsd_context p;
    assert(nxbsd_context_init(&p,"sdmc:/")==0);
    size_t total=0;
    int status[3]={-1,-1,-1};
    struct nxbsd_stage s[]={{produce,NULL},{transform,NULL},{consume,&total}};
    assert(nxbsd_pipeline_run(&p,s,3,status)==0);
    assert(total==TOTAL && status[0]==3 && status[1]==4 && status[2]==5);
    struct nxbsd_stage stop[]={{produce,NULL},{early,NULL}};
    assert(nxbsd_pipeline_run(&p,stop,2,status)==0);
    assert(status[0]==17 && status[1]==9);
    int ends[2];
    assert(nxbsd_fd_pipe(&p.fd,ends)==0);
    for(int i=2;i<NXBSD_FD_MAX;i++) assert(nxbsd_fd_dup2(&p.fd,ends[1],i)==i);
    atomic_int called=0;
    struct nxbsd_stage full[]={{count,&called},{count,&called}};
    status[0]=-1; status[1]=-1;
    assert(nxbsd_pipeline_run(&p,full,2,status)==-1 && errno==EMFILE);
    assert(atomic_load(&called)==0 && status[0]==-1 && status[1]==-1);
    nxbsd_context_destroy(&p);
    puts("NX-BSD pipeline contracts passed");
    return 0;
}
