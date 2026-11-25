#ifndef EXODUS_NXBSD_TASK_H
#define EXODUS_NXBSD_TASK_H
#include "nxbsd_context.h"
#include <pthread.h>
#include <stdint.h>
#define NXBSD_TASK_MAX 16
struct nxbsd_task_id { unsigned slot; uint64_t generation; };
typedef int (*nxbsd_task_fn)(struct nxbsd_context *, void *);
enum nxbsd_task_state { NXBSD_TASK_FREE, NXBSD_TASK_RUNNING, NXBSD_TASK_EXITED, NXBSD_TASK_JOINING };
struct nxbsd_task {
    pthread_t thread;
    struct nxbsd_context *ctx;
    nxbsd_task_fn fn;
    void *arg;
    struct nxbsd_scheduler *owner;
    uint64_t generation;
    enum nxbsd_task_state state;
    int status;
};
struct nxbsd_scheduler {
    pthread_mutex_t lock;
    struct nxbsd_task tasks[NXBSD_TASK_MAX];
};
/* Trusted callbacks only, same address space. No fork or signal emulation.
 * Caller owns arg until successful wait. Callbacks must return normally.
 * Source context must remain stable during clone. One successful wait
 * consumes the token. Scheduler destruction requires no concurrent callers. */
int nxbsd_scheduler_init(struct nxbsd_scheduler *);
int nxbsd_scheduler_destroy(struct nxbsd_scheduler *);
int nxbsd_task_spawn(struct nxbsd_scheduler *, const struct nxbsd_context *,
                     nxbsd_task_fn, void *, struct nxbsd_task_id *);
int nxbsd_task_poll(struct nxbsd_scheduler *, struct nxbsd_task_id, int *exited);
int nxbsd_task_wait(struct nxbsd_scheduler *, struct nxbsd_task_id, int *status, int nonblock);
#endif
