#include "reactor.h"
#include <errno.h>

int nx_reactor_wait(const struct nx_watch *watches, size_t count,
                    struct nx_ready *ready, size_t capacity, int timeout_ms)
{
    struct pollfd fds[NX_REACTOR_MAX];
    size_t i, j, n = 0;
    int rc;
    if (!count || count > NX_REACTOR_MAX || !watches || !ready ||
        capacity < count || timeout_ms < -1) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < count; i++) {
        if (watches[i].fd < 0 || !watches[i].events ||
            (watches[i].events & ~(POLLIN | POLLOUT))) {
            errno = EINVAL;
            return -1;
        }
        for (j = 0; j < i; j++) {
            if (watches[j].token == watches[i].token || watches[j].fd == watches[i].fd) {
                errno = EINVAL;
                return -1;
            }
        }
        fds[i] = (struct pollfd){watches[i].fd, watches[i].events, 0};
    }
    rc = poll(fds, (nfds_t)count, timeout_ms);
    if (rc < 0) return -1; /* EINTR is visible to the caller. */
    for (i = 0; i < count; i++) {
        if (fds[i].revents) {
            ready[n++] = (struct nx_ready){watches[i].token, fds[i].revents};
        }
    }
    return (int)n;
}
