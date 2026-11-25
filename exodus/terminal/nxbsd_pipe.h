#ifndef EXODUS_NXBSD_PIPE_H
#define EXODUS_NXBSD_PIPE_H
#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>
#define NXBSD_PIPE_CAPACITY 4096
/* Each successful init creates one read and one write reference.
 * Retain before sharing an endpoint with another owner. Each owner closes
 * once, after its operations finish. Destroy only after all owners join.
 * This is a byte-channel provider, not a global POSIX descriptor table.
 * No signal delivery, cancellation, or cross-process sharing yet. */
struct nxbsd_pipe {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    size_t head, used;
    unsigned readers, writers;
    unsigned char data[NXBSD_PIPE_CAPACITY];
};
int nxbsd_pipe_init(struct nxbsd_pipe *p);
int nxbsd_pipe_retain(struct nxbsd_pipe *p, int write_end);
int nxbsd_pipe_close(struct nxbsd_pipe *p, int write_end);
int nxbsd_pipe_destroy(struct nxbsd_pipe *p);
ssize_t nxbsd_pipe_read(struct nxbsd_pipe *p, void *buf, size_t len, int nonblock);
ssize_t nxbsd_pipe_write(struct nxbsd_pipe *p, const void *buf, size_t len, int nonblock);
#endif
