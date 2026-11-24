/* Target compile inventory only. No Linux ABI or fake syscall declarations. */
#ifndef EXODUS_FREEBSD_HORIZON_COMPILE_H
#define EXODUS_FREEBSD_HORIZON_COMPILE_H
#include <sys/cdefs.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/param.h>
#ifndef MAXLOGNAME
#define MAXLOGNAME 33 /* pinned FreeBSD sys/sys/param.h */
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
#ifndef ALIGN
#define ALIGN(p) (((p) + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))
#endif
#ifndef __DECONST
#define __DECONST(type,value) ((type)(uintptr_t)(const void *)(value))
#endif
#endif
