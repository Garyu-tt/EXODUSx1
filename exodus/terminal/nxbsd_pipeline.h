#ifndef EXODUS_NXBSD_PIPELINE_H
#define EXODUS_NXBSD_PIPELINE_H
#include "nxbsd_task.h"
#define NXBSD_PIPELINE_MAX 8
struct nxbsd_stage {
    nxbsd_task_fn fn;
    void *arg;
};
/* Synchronous pipeline over trusted callbacks. Each stage receives a cloned
 * context, fd 0/1 routed to neighbouring stages; outer stdin/stdout/stderr
 * are inherited. Caller owns all args until return. Callbacks must cooperate:
 * return on read/write errors and must not wait on an external event forever.
 * status[] receives all stage exit codes, not POSIX encoded wait statuses.
 * No global fd manipulation, shell parser integration or fork emulation. */
int nxbsd_pipeline_run(const struct nxbsd_context *parent,
                       const struct nxbsd_stage *stages, size_t n, int *status);
#endif
