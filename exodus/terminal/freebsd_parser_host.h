/* Compile probe only; no target syscall or security emulation. */
#ifndef EXODUS_FREEBSD_PARSER_HOST_H
#define EXODUS_FREEBSD_PARSER_HOST_H
#include <sys/cdefs.h>
#include <stddef.h>
#include <string.h>
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
/* Declaration only: object compilation does not establish link/runtime availability. */
size_t strlcpy(char *, const char *, size_t);
#endif
