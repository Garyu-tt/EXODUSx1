#include "nxbsd_pipe.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

static void endpoints(void)
{
    struct nxbsd_pipe p;
    unsigned char a[NXBSD_PIPE_CAPACITY], b[NXBSD_PIPE_CAPACITY];
    memset(a, 0xab, sizeof(a));
    assert(nxbsd_pipe_init(&p) == 0);
    assert(nxbsd_pipe_destroy(&p) == -1 && errno == EBUSY);
    assert(nxbsd_pipe_read(&p,b,1,1) == -1 && errno == EAGAIN);
    assert(nxbsd_pipe_write(&p,a,sizeof(a),1) == sizeof(a));
    assert(nxbsd_pipe_read(&p,b,100,1) == 100);
    assert(nxbsd_pipe_write(&p,a,101,1) == -1 && errno == EAGAIN);
    assert(nxbsd_pipe_write(&p,a,100,1) == 100);
    assert(nxbsd_pipe_retain(&p,1) == 0);
    assert(nxbsd_pipe_close(&p,1) == 0);
    assert(nxbsd_pipe_read(&p,b,sizeof(b),0) == sizeof(b));
    assert(memcmp(a,b,sizeof(a)) == 0);
    assert(nxbsd_pipe_read(&p,b,1,1) == -1 && errno == EAGAIN);
    assert(nxbsd_pipe_close(&p,1) == 0);
    assert(nxbsd_pipe_read(&p,b,1,0) == 0);
    assert(nxbsd_pipe_retain(&p,1) == -1 && errno == EBADF);
    assert(nxbsd_pipe_close(&p,0) == 0);
    assert(nxbsd_pipe_destroy(&p) == 0);
    assert(nxbsd_pipe_init(&p) == 0);
    assert(nxbsd_pipe_close(&p,0) == 0);
    assert(nxbsd_pipe_write(&p,a,1,0) == -1 && errno == EPIPE);
    assert(nxbsd_pipe_close(&p,1) == 0);
    assert(nxbsd_pipe_destroy(&p) == 0);
}

static void *producer(void *arg)
{
    struct nxbsd_pipe *p = arg;
    unsigned char bytes[19001];
    for (size_t i=0;i<sizeof(bytes);++i) bytes[i]=(unsigned char)(i%251);
    assert(nxbsd_pipe_write(p,bytes,sizeof(bytes),0)==sizeof(bytes));
    assert(nxbsd_pipe_close(p,1)==0);
    return NULL;
}

static void transfer(void)
{
    struct nxbsd_pipe p;
    pthread_t thread;
    unsigned char buf[333];
    size_t total=0;
    assert(nxbsd_pipe_init(&p)==0);
    assert(pthread_create(&thread,NULL,producer,&p)==0);
    for (;;) {
        ssize_t n=nxbsd_pipe_read(&p,buf,sizeof(buf),0);
        assert(n>=0);
        if (!n) break;
        for (ssize_t i=0;i<n;++i) assert(buf[i]==(unsigned char)((total+(size_t)i)%251));
        total+=(size_t)n;
    }
    assert(total==19001);
    assert(pthread_join(thread,NULL)==0);
    assert(nxbsd_pipe_close(&p,0)==0);
    assert(nxbsd_pipe_destroy(&p)==0);
}

int main(void)
{
    endpoints();
    transfer();
    puts("NX-BSD pipe contracts passed");
    return 0;
}
