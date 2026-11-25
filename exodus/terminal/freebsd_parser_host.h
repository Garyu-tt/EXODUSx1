/* Compile probe only; no target syscall or security emulation. */
#ifndef EXODUS_FREEBSD_PARSER_HOST_H
#define EXODUS_FREEBSD_PARSER_HOST_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
/* Linux monotonic clock excludes suspend, like FreeBSD CLOCK_UPTIME. */
#ifndef CLOCK_UPTIME
#define CLOCK_UPTIME CLOCK_MONOTONIC
#endif
#ifndef timespecsub
#define timespecsub(a,b,r) do { \
    (r)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (r)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
    if ((r)->tv_nsec < 0) { --(r)->tv_sec; (r)->tv_nsec += 1000000000L; } \
} while (0)
#endif
#ifndef timespeccmp
#define timespeccmp(a,b,op) (((a)->tv_sec == (b)->tv_sec) ? \
    ((a)->tv_nsec op (b)->tv_nsec) : ((a)->tv_sec op (b)->tv_sec))
#endif
/* Real implementations to be supplied by host libbsd at final link. */
void *setmode(const char *);
mode_t getmode(const void *, mode_t);
int tcsetsid(int, pid_t);
/* Host-only implementations in freebsd_stdio_host.c. */
int eaccess(const char *, int);
const char *nxbsd_signal_name(int);
#define NXBSD_SIG2STR_MAX 32
int nxbsd_sig2str(int, char *);
int nxbsd_str2sig(const char *, int *);
FILE *fwopen(void *, int (*)(void *, const char *, int));
#include <sys/param.h>
/* FreeBSD sys/sys/param.h at the locked revision: length includes NUL. */
#ifndef MAXLOGNAME
#define MAXLOGNAME 33
#endif
#ifndef __dead2
#define __dead2 __attribute__((noreturn))
#endif
#ifndef __printflike
#define __printflike(a,b) __attribute__((format(printf,a,b)))
#endif
#ifndef __printf0like
#define __printf0like(a,b) __attribute__((format(printf,a,b)))
#endif
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#ifndef __nonstring
#define __nonstring __attribute__((nonstring))
#endif
/* Host object probe: FreeBSD sys/sys/_align.h aligns to pointer width. */
#ifndef ALIGN
#define ALIGN(p) (((p) + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))
#endif
#ifndef __DECONST
#define __DECONST(type, value) ((type)(uintptr_t)(const void *)(value))
#endif
/* Declaration only: object compilation does not establish link/runtime availability. */
size_t strlcpy(char *, const char *, size_t);
#endif
