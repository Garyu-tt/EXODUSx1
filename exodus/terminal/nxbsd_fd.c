#include "nxbsd_fd.h"
#include "nxbsd_pipe.h"
#include <errno.h>
#include <stdatomic.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum kind { NATIVE, PIPE_READ, PIPE_WRITE };
struct pipe_pair { struct nxbsd_pipe pipe; atomic_uint ends; };
struct nxbsd_fd_object {
    enum kind kind;
    atomic_uint refs;
    int native;
    struct pipe_pair *pair;
};

static int bad(int e) { errno = e; return -1; }
static int valid(int fd) { return fd >= 0 && fd < NXBSD_FD_MAX; }
static struct nxbsd_fd_object *lookup(struct nxbsd_fd_table *t, int fd)
{
    if (!t || !valid(fd) || !t->slot[fd]) { errno = EBADF; return NULL; }
    return t->slot[fd];
}
static int empty(struct nxbsd_fd_table *t, int start)
{
    for (int i = start; i < NXBSD_FD_MAX; ++i)
        if (!t->slot[i]) return i;
    return bad(EMFILE);
}
void nxbsd_fd_init(struct nxbsd_fd_table *t)
{
    memset(t, 0, sizeof(*t));
}
static int retain(struct nxbsd_fd_object *o)
{
    unsigned n = atomic_load(&o->refs);
    do {
        if (n == UINT_MAX) return bad(EOVERFLOW);
    } while (!atomic_compare_exchange_weak(&o->refs, &n, n + 1));
    return 0;
}
int nxbsd_fd_clone(struct nxbsd_fd_table *dst, const struct nxbsd_fd_table *src)
{
    if (!dst || !src || dst == src) return bad(EINVAL);
    for (int i = 0; i < NXBSD_FD_MAX; ++i)
        if (dst->slot[i]) return bad(EBUSY);
    for (int i = 0; i < NXBSD_FD_MAX; ++i) {
        if (!src->slot[i]) continue;
        if (retain(src->slot[i])) {
            int e = errno;
            nxbsd_fd_clear(dst);
            return bad(e);
        }
        dst->slot[i] = src->slot[i];
    }
    return 0;
}
int nxbsd_fd_adopt(struct nxbsd_fd_table *t, int fd)
{
    if (!t || fd < 0) return bad(EINVAL);
    for (int i = 0; i < NXBSD_FD_MAX; ++i)
        if (t->slot[i] && t->slot[i]->kind == NATIVE &&
            t->slot[i]->native == fd) return bad(EEXIST);
    if (fcntl(fd, F_GETFD) < 0) return -1;
    int n = empty(t, 0);
    if (n < 0) return -1;
    struct nxbsd_fd_object *o = calloc(1, sizeof(*o));
    if (!o) return -1;
    o->kind = NATIVE; atomic_init(&o->refs, 1); o->native = fd;
    t->slot[n] = o;
    return n;
}
int nxbsd_fd_pipe(struct nxbsd_fd_table *t, int ends[2])
{
    return nxbsd_fd_pipe_from(t, 0, ends);
}
int nxbsd_fd_pipe_from(struct nxbsd_fd_table *t, int minimum, int ends[2])
{
    if (!t || !ends || !valid(minimum)) return bad(EINVAL);
    int r = empty(t, minimum);
    if (r < 0) return -1;
    int w = empty(t, r+1);
    if (w < 0) return -1;
    struct pipe_pair *p = calloc(1, sizeof(*p));
    struct nxbsd_fd_object *a = calloc(1, sizeof(*a));
    struct nxbsd_fd_object *b = calloc(1, sizeof(*b));
    if (!p || !a || !b) {
        free(p); free(a); free(b); return bad(ENOMEM);
    }
    if (nxbsd_pipe_init(&p->pipe)) {
        int e = errno; free(p); free(a); free(b); return bad(e);
    }
    atomic_init(&p->ends, 2);
    a->kind = PIPE_READ; atomic_init(&a->refs, 1); a->pair = p;
    b->kind = PIPE_WRITE; atomic_init(&b->refs, 1); b->pair = p;
    t->slot[r] = a; t->slot[w] = b;
    ends[0] = r; ends[1] = w;
    return 0;
}
int nxbsd_fd_close(struct nxbsd_fd_table *t, int fd)
{
    struct nxbsd_fd_object *o = lookup(t, fd);
    if (!o) return -1;
    t->slot[fd] = NULL;
    if (atomic_fetch_sub(&o->refs, 1) != 1) return 0;
    int rc = 0;
    if (o->kind == NATIVE) rc = close(o->native);
    else {
        struct pipe_pair *p = o->pair;
        rc = nxbsd_pipe_close(&p->pipe, o->kind == PIPE_WRITE);
        if (atomic_fetch_sub(&p->ends, 1) == 1) {
            if (nxbsd_pipe_destroy(&p->pipe) == 0) free(p);
            else rc = -1;
        }
    }
    int e = errno;
    free(o);
    errno = e;
    return rc;
}
int nxbsd_fd_dup2(struct nxbsd_fd_table *t, int oldfd, int newfd)
{
    struct nxbsd_fd_object *o = lookup(t, oldfd);
    if (!o || !valid(newfd)) return bad(EBADF);
    if (oldfd == newfd) return newfd;
    if (retain(o)) return -1;
    if (t->slot[newfd]) (void)nxbsd_fd_close(t, newfd);
    t->slot[newfd] = o;
    return newfd;
}
void nxbsd_fd_clear(struct nxbsd_fd_table *t)
{
    if (!t) return;
    for (int i = 0; i < NXBSD_FD_MAX; ++i)
        if (t->slot[i]) (void)nxbsd_fd_close(t, i);
}
ssize_t nxbsd_fd_read(struct nxbsd_fd_table *t, int fd, void *buf, size_t n, int nonblock)
{
    struct nxbsd_fd_object *o = lookup(t, fd);
    if (!o) return -1;
    if (o->kind == PIPE_READ) return nxbsd_pipe_read(&o->pair->pipe, buf, n, nonblock);
    if (o->kind != NATIVE) return bad(EBADF);
    /* Never change shared native open-file flags behind the owner's back. */
    if (nonblock) return bad(ENOTSUP);
    return read(o->native, buf, n);
}
ssize_t nxbsd_fd_write(struct nxbsd_fd_table *t, int fd, const void *buf, size_t n, int nonblock)
{
    struct nxbsd_fd_object *o = lookup(t, fd);
    if (!o) return -1;
    if (o->kind == PIPE_WRITE) return nxbsd_pipe_write(&o->pair->pipe, buf, n, nonblock);
    if (o->kind != NATIVE) return bad(EBADF);
    if (nonblock) return bad(ENOTSUP);
    return write(o->native, buf, n);
}
