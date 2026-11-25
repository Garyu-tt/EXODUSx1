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
        if (watches[i].events & ~(POLLIN | POLLPRI | POLLOUT)) {
            errno = EINVAL;
            return -1;
        }
        for (j = 0; j < i; j++) {
            if (watches[j].token == watches[i].token) {
                errno = EINVAL;
                return -1;
            }
        }
        /* poll() deliberately permits negative and duplicate descriptors.
         * Negative fds are ignored; duplicate fds produce one result per watch. */
        fds[i] = (struct pollfd){watches[i].fd, watches[i].events, 0};
    }

    rc = poll(fds, (nfds_t)count, timeout_ms);
    if (rc < 0) return -1; /* EINTR and backend errors remain visible. */

    for (i = 0; i < count; i++) {
        if (fds[i].revents)
            ready[n++] = (struct nx_ready){watches[i].token, fds[i].revents};
    }
    return (int)n;
}
