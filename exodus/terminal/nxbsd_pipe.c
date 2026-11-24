#include "nxbsd_pipe.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
static int bad(int e) { errno = e; return -1; }
int nxbsd_pipe_init(struct nxbsd_pipe *p)
{
    if (!p) return bad(EINVAL);
    memset(p, 0, sizeof(*p));
    int e = pthread_mutex_init(&p->lock, NULL);
    if (e) return bad(e);
    e = pthread_cond_init(&p->changed, NULL);
    if (e) { pthread_mutex_destroy(&p->lock); return bad(e); }
    p->readers = p->writers = 1;
    return 0;
}
int nxbsd_pipe_retain(struct nxbsd_pipe *p, int w)
{
    if (!p || (w != 0 && w != 1)) return bad(EINVAL);
    pthread_mutex_lock(&p->lock);
    unsigned *n = w ? &p->writers : &p->readers;
    int e = !*n ? EBADF : *n == UINT_MAX ? EOVERFLOW : 0;
    if (!e) ++*n;
    pthread_mutex_unlock(&p->lock);
    return e ? bad(e) : 0;
}
int nxbsd_pipe_close(struct nxbsd_pipe *p, int w)
{
    if (!p || (w != 0 && w != 1)) return bad(EINVAL);
    pthread_mutex_lock(&p->lock);
    unsigned *n = w ? &p->writers : &p->readers;
    if (!*n) { pthread_mutex_unlock(&p->lock); return bad(EBADF); }
    --*n;
    pthread_cond_broadcast(&p->changed);
    pthread_mutex_unlock(&p->lock);
    return 0;
}
int nxbsd_pipe_destroy(struct nxbsd_pipe *p)
{
    if (!p) return bad(EINVAL);
    /* Caller guarantees no concurrent operation or future retain. */
    if (p->readers || p->writers) return bad(EBUSY);
    int e = pthread_cond_destroy(&p->changed);
    if (e) return bad(e);
    e = pthread_mutex_destroy(&p->lock);
    return e ? bad(e) : 0;
}
ssize_t nxbsd_pipe_read(struct nxbsd_pipe *p, void *buf, size_t len, int nonblock)
{
    if (!p || (!buf && len)) return bad(EINVAL);
    pthread_mutex_lock(&p->lock);
    if (!p->readers) { pthread_mutex_unlock(&p->lock); return bad(EBADF); }
    if (!len) { pthread_mutex_unlock(&p->lock); return 0; }
    while (!p->used && p->writers) {
        if (nonblock) { pthread_mutex_unlock(&p->lock); return bad(EAGAIN); }
        int e = pthread_cond_wait(&p->changed, &p->lock);
        if (e) { pthread_mutex_unlock(&p->lock); return bad(e); }
    }
    size_t n = len < p->used ? len : p->used;
    size_t first = NXBSD_PIPE_CAPACITY - p->head;
    if (first > n) first = n;
    if (n) {
        memcpy(buf, p->data + p->head, first);
        memcpy((unsigned char *)buf + first, p->data, n - first);
        p->head = (p->head + n) % NXBSD_PIPE_CAPACITY;
        p->used -= n;
        pthread_cond_broadcast(&p->changed);
    }
    pthread_mutex_unlock(&p->lock);
    return (ssize_t)n;
}
ssize_t nxbsd_pipe_write(struct nxbsd_pipe *p, const void *buf, size_t len, int nonblock)
{
    if (!p || (!buf && len) || len > (size_t)SSIZE_MAX) return bad(EINVAL);
    pthread_mutex_lock(&p->lock);
    if (!p->writers) { pthread_mutex_unlock(&p->lock); return bad(EBADF); }
    size_t done = 0;
    while (done < len) {
        if (!p->readers) { pthread_mutex_unlock(&p->lock); return done ? (ssize_t)done : bad(EPIPE); }
        size_t avail = NXBSD_PIPE_CAPACITY - p->used;
        size_t rem = len - done;
        /* Small writes are atomic relative to other writers. */
        size_t need = len <= NXBSD_PIPE_CAPACITY ? rem : 1;
        if (avail < need) {
            if (nonblock) { pthread_mutex_unlock(&p->lock); return done ? (ssize_t)done : bad(EAGAIN); }
            int e = pthread_cond_wait(&p->changed, &p->lock);
            if (e) { pthread_mutex_unlock(&p->lock); return done ? (ssize_t)done : bad(e); }
            continue;
        }
        size_t n = rem < avail ? rem : avail;
        size_t tail = (p->head + p->used) % NXBSD_PIPE_CAPACITY;
        size_t first = NXBSD_PIPE_CAPACITY - tail;
        if (first > n) first = n;
        memcpy(p->data + tail, (const unsigned char *)buf + done, first);
        memcpy(p->data, (const unsigned char *)buf + done + first, n - first);
        p->used += n;
        done += n;
        pthread_cond_broadcast(&p->changed);
    }
    pthread_mutex_unlock(&p->lock);
    return (ssize_t)done;
}
