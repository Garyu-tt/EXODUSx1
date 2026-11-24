/* Compile probe only; no target syscall or security emulation. */
#ifndef EXODUS_FREEBSD_PARSER_HOST_H
#define EXODUS_FREEBSD_PARSER_HOST_H
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
/* Host-only implementations in freebsd_stdio_host.c. */
int eaccess(const char *, int);
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
