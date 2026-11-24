#ifndef EXODUS_NXBSD_CONTEXT_H
#define EXODUS_NXBSD_CONTEXT_H
#include "nxbsd_fd.h"
#define NXBSD_ENV_MAX 32
#define NXBSD_ENV_KEY 64
#define NXBSD_ENV_VALUE 512
#define NXBSD_CWD_MAX 1024
struct nxbsd_env {
    char key[NXBSD_ENV_KEY];
    char value[NXBSD_ENV_VALUE];
};
struct nxbsd_context {
    struct nxbsd_fd_table fd;
    struct nxbsd_env env[NXBSD_ENV_MAX];
    char cwd[NXBSD_CWD_MAX];
    int status;
};
/* One owner per context. No libc-global chdir/setenv; cwd is metadata,
 * not a sandbox boundary or a validated filesystem location.
 * Clone into a distinct, uninitialized context; destroy each once.
 * Cloned descriptors share underlying open file descriptions. */
int nxbsd_context_init(struct nxbsd_context *ctx, const char *cwd);
int nxbsd_context_clone(struct nxbsd_context *dst, const struct nxbsd_context *src);
void nxbsd_context_destroy(struct nxbsd_context *ctx);
int nxbsd_context_setenv(struct nxbsd_context *ctx, const char *key, const char *value);
const char *nxbsd_context_getenv(const struct nxbsd_context *ctx, const char *key);
int nxbsd_context_unsetenv(struct nxbsd_context *ctx, const char *key);
int nxbsd_context_setcwd(struct nxbsd_context *ctx, const char *cwd);
#endif
