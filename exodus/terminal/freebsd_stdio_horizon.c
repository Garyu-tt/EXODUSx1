#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

struct writer {
    void *cookie;
    int (*write)(void *, const char *, int);
};
static int write_cookie(void *opaque, const char *buf, size_t size)
{
    struct writer *w=opaque;
    if(size>INT_MAX) { errno=EOVERFLOW; return -1; }
    size_t done=0;
    while(done<size) {
        int n=(int)(size-done);
        int rc=w->write(w->cookie,buf+done,n);
        if(rc<=0 || rc>n) {
            if(rc>=0) errno=EIO;
            return done?(int)done:-1;
        }
        done+=(size_t)rc;
    }
    return (int)done;
}
static int close_cookie(void *opaque)
{
    free(opaque);
    return 0;
}
FILE *nxbsd_fwopen(void *cookie,int (*writefn)(void *,const char *,int))
{
    if(!writefn) { errno=EINVAL; return NULL; }
    struct writer *w=malloc(sizeof(*w));
    if(!w) return NULL;
    *w=(struct writer){cookie,writefn};
    FILE *f=funopen(w,NULL,write_cookie,NULL,close_cookie);
    if(!f) free(w);
    return f;
}
char *nxbsd_strchrnul(const char *s,int c)
{
    const unsigned char wanted=(unsigned char)c;
    while(*s && (unsigned char)*s!=wanted) ++s;
    return (char *)s;
}
