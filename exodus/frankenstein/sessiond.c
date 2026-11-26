#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Local, unprivileged reference daemon. Desired mode only; no compositor launch. */
struct config { uint64_t generation; char mode[8]; };
static int read_config(const char *path, struct config *out) {
    char buf[128], *p, *end;
    struct stat st;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) return 0;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & 022) || st.st_size <= 0 || st.st_size >= (off_t)sizeof(buf)) {
        close(fd); return 0;
    }
    size_t used = 0;
    while (used < sizeof(buf)-1) {
        ssize_t n = read(fd, buf+used, sizeof(buf)-1-used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { close(fd); return 0; }
        if (!n) break;
        used += (size_t)n;
    }
    char extra;
    ssize_t n = read(fd, &extra, 1);
    close(fd);
    if (n != 0 || memchr(buf, 0, used)) return 0;
    buf[used] = 0;
    if (strncmp(buf, "generation=", 11)) return 0;
    p = buf + 11;
    if (*p < '1' || *p > '9') return 0;
    errno = 0;
    uintmax_t gen = strtoumax(p, &end, 10);
    if (errno || gen > UINT64_MAX || strncmp(end, "\nmode=", 6)) return 0;
    p = end + 6;
    if (strcmp(p, "game\n") && strcmp(p, "desktop\n")) return 0;
    out->generation = (uint64_t)gen;
    snprintf(out->mode, sizeof(out->mode), "%s", !strcmp(p,"game\n") ? "game" : "desktop");
    return 1;
}
static void report(const char *event, const struct config *c) {
    printf("%s generation=%" PRIu64 " desired=%s active=unknown pid=%ld\n",
           event, c->generation, c->mode, (long)getpid());
    fflush(stdout);
}
#ifdef EXODUS_DBUS
#include "sessiond-dbus.inc"
#else
int main(int argc, char **argv) {
    if (argc != 2) { fputs("usage: exodus-sessiond CONFIG\n", stderr); return 2; }
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGHUP); sigaddset(&signals, SIGUSR1); sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, NULL)) return 1;
    /* rc/su can inherit ignored signals. FreeBSD may discard them even
     * when blocked. Restore default dispositions before synchronous wait. */
    struct sigaction sa = {0};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) || sigaction(SIGUSR1, &sa, NULL) ||
        sigaction(SIGTERM, &sa, NULL)) return 1;
    struct config active, candidate;
    if (!read_config(argv[1], &active)) { fputs("invalid initial config\n", stderr); return 1; }
    report("ready", &active);
    for (;;) {
        int sig;
        if (sigwait(&signals, &sig)) return 1;
        if (sig == SIGTERM) { report("stopped", &active); return 0; }
        if (sig == SIGUSR1) report("status", &active);
        if (sig == SIGHUP) {
            if (read_config(argv[1], &candidate) && candidate.generation > active.generation) {
                active = candidate; report("reloaded", &active);
            } else report("rejected", &active);
        }
    }
}

#endif
