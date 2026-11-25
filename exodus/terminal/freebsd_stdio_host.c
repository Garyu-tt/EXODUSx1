/* Host reference only: BSD fwopen subset used by FreeBSD sh output.c. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>

struct writer {
    void *cookie;
    int (*write)(void *, const char *, int);
};

static ssize_t write_cookie(void *opaque, const char *buf, size_t size)
{
    struct writer *w = opaque;
    size_t done = 0;
    if (size > (size_t)SSIZE_MAX) { errno = EOVERFLOW; return -1; }
    while (done < size) {
        int n = size - done > INT_MAX ? INT_MAX : (int)(size - done);
        int rc = w->write(w->cookie, buf + done, n);
        if (rc <= 0 || rc > n) {
            if (rc >= 0) errno = EIO;
            return done ? (ssize_t)done : -1;
        }
        done += (size_t)rc;
    }
    return (ssize_t)done;
}

static int close_cookie(void *opaque)
{
    free(opaque);
    return 0;
}

FILE *fwopen(void *cookie, int (*writefn)(void *, const char *, int))
{
    if (!writefn) { errno = EINVAL; return NULL; }
    struct writer *w = malloc(sizeof(*w));
    if (!w) return NULL;
    *w = (struct writer){cookie, writefn};
    cookie_io_functions_t io = {.write = write_cookie, .close = close_cookie};
    FILE *f = fopencookie(w, "w", io);
    if (!f) free(w);
    return f;
}

#include <unistd.h>
#include <fcntl.h>
/* Preserve effective credentials; never substitute real-ID access(). */
int eaccess(const char *path, int mode)
{
    return faccessat(AT_FDCWD, path, mode, AT_EACCESS);
}

#include <signal.h>
#include <string.h>
/* Unnamed signals (including realtime signals without abbreviations) stay unknown. */
const char *nxbsd_signal_name(int number)
{
    return number > 0 && number < NSIG ? sigabbrev_np(number) : NULL;
}

/* Named signals supported by the host; do not invent BSD numbers.
   Realtime numeric signals may be passed to kill, but names are not synthesized. */
int nxbsd_sig2str(int n, char *out)
{
    const char *name = nxbsd_signal_name(n);
    if (!out || !name || strlen(name) >= 32) { errno = EINVAL; return -1; }
    strcpy(out, name);
    return 0;
}
int nxbsd_str2sig(const char *s, int *out)
{
    if (!s || !*s || !out) { errno = EINVAL; return -1; }
    if (*s >= '0' && *s <= '9') {
        char *end;
        errno = 0;
        long n = strtol(s, &end, 10);
        if (!errno && !*end && n > 0 && n < NSIG &&
            (nxbsd_signal_name((int)n) || (n >= SIGRTMIN && n <= SIGRTMAX))) {
            *out = (int)n;
            return 0;
        }
    } else {
        for (int n = 1; n < NSIG; ++n) {
            const char *name = nxbsd_signal_name(n);
            if (name && !strcasecmp(s, name)) { *out = n; return 0; }
        }
    }
    errno = EINVAL;
    return -1;
}

#include <sys/ioctl.h>
/* Only acquire a controlling terminal for the caller's own session.
   Never request forced takeover of another session's terminal. */
int tcsetsid(int fd, pid_t sid)
{
    if (sid != getpid() || sid != getsid(0)) { errno = EPERM; return -1; }
    return ioctl(fd, TIOCSCTTY, 0);
}
