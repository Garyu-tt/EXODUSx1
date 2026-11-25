#ifndef EXODUS_NXBSD_FD_H
#define EXODUS_NXBSD_FD_H
#include <stddef.h>
#include <sys/types.h>
#define NXBSD_FD_MAX 64
struct nxbsd_fd_object;
struct nxbsd_fd_table {
    struct nxbsd_fd_object *slot[NXBSD_FD_MAX];
};
/* Initialize once, before use. Table mutations require exclusive ownership
 * and no concurrent I/O. Concurrent I/O through stable slots is allowed.
 * Native descriptors transfer ownership only on successful adoption.
 * Do not close adopted descriptors outside this provider.
 * Independent namespace: virtual fd != native fd or Horizon handle.
 * Separate tables may share objects via clone; never adopt an already-owned
 * native fd into another table. No signal delivery or libc interposition yet. */
void nxbsd_fd_init(struct nxbsd_fd_table *);
/* dst must be initialized and empty; source table must remain stable. */
int nxbsd_fd_clone(struct nxbsd_fd_table *dst, const struct nxbsd_fd_table *src);
int nxbsd_fd_adopt(struct nxbsd_fd_table *, int native_fd);
int nxbsd_fd_pipe_from(struct nxbsd_fd_table *, int minimum, int ends[2]);
int nxbsd_fd_pipe(struct nxbsd_fd_table *, int ends[2]);
int nxbsd_fd_dup2(struct nxbsd_fd_table *, int oldfd, int newfd);
int nxbsd_fd_close(struct nxbsd_fd_table *, int fd);
void nxbsd_fd_clear(struct nxbsd_fd_table *);
ssize_t nxbsd_fd_read(struct nxbsd_fd_table *, int fd, void *, size_t, int nonblock);
ssize_t nxbsd_fd_write(struct nxbsd_fd_table *, int fd, const void *, size_t, int nonblock);
#endif
