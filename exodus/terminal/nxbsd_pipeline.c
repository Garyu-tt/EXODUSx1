#include "nxbsd_pipeline.h"
#include <errno.h>
#include <stdlib.h>

struct gate {
    pthread_mutex_t lock;
    pthread_cond_t cv;
    int decision; /* zero: pending, 1: start, -1: abort */
};
struct stage_run {
    struct gate *gate;
    struct nxbsd_stage stage;
};
static int bad(int e) { errno=e; return -1; }
static int gated_stage(struct nxbsd_context *ctx, void *arg)
{
    struct stage_run *r=arg;
    pthread_mutex_lock(&r->gate->lock);
    while(!r->gate->decision)
        pthread_cond_wait(&r->gate->cv,&r->gate->lock);
    int go=r->gate->decision;
    pthread_mutex_unlock(&r->gate->lock);
    return go>0 ? r->stage.fn(ctx,r->stage.arg) : 125;
}

int nxbsd_pipeline_run(const struct nxbsd_context *parent,
                       const struct nxbsd_stage *stages,size_t n,int *status)
{
    if(!parent || !stages || !status || !n || n>NXBSD_PIPELINE_MAX)
        return bad(EINVAL);
    for(size_t i=0;i<n;i++) if(!stages[i].fn) return bad(EINVAL);
    struct nxbsd_context *master=malloc(sizeof(*master));
    struct nxbsd_context *stage=malloc(sizeof(*stage));
    if(!master || !stage) { free(master); free(stage); return bad(ENOMEM); }
    int e=0;
    if(nxbsd_context_clone(master,parent)) {
        e=errno; free(master); free(stage); return bad(e);
    }
    int pipes[NXBSD_PIPELINE_MAX-1][2];
    for(size_t i=0;i+1<n;i++)
        if(nxbsd_fd_pipe_from(&master->fd,3,pipes[i])) {
            e=errno; goto cleanup_context;
        }
    struct nxbsd_scheduler s;
    if(nxbsd_scheduler_init(&s)) { e=errno; goto cleanup_context; }
    struct gate g={0};
    e=pthread_mutex_init(&g.lock,NULL);
    if(e) goto cleanup_scheduler;
    e=pthread_cond_init(&g.cv,NULL);
    if(e) goto cleanup_mutex;
    struct stage_run runs[NXBSD_PIPELINE_MAX];
    struct nxbsd_task_id ids[NXBSD_PIPELINE_MAX];
    size_t started=0;
    for(size_t i=0;i<n;i++) {
        if(nxbsd_context_clone(stage,master)) { e=errno; break; }
        if((i && nxbsd_fd_dup2(&stage->fd,pipes[i-1][0],0)<0) ||
           (i+1<n && nxbsd_fd_dup2(&stage->fd,pipes[i][1],1)<0)) {
            e=errno; nxbsd_context_destroy(stage); break;
        }
        for(size_t j=0;j+1<n;j++) {
            nxbsd_fd_close(&stage->fd,pipes[j][0]);
            nxbsd_fd_close(&stage->fd,pipes[j][1]);
        }
        runs[i]=(struct stage_run){&g,stages[i]};
        if(nxbsd_task_spawn(&s,stage,gated_stage,&runs[i],&ids[i]))
            e=errno;
        nxbsd_context_destroy(stage);
        if(e) break;
        started++;
    }
    /* Drop setup references before allowing any callback to read EOF. */
    nxbsd_context_destroy(master);
    pthread_mutex_lock(&g.lock);
    g.decision=e ? -1 : 1;
    pthread_cond_broadcast(&g.cv);
    pthread_mutex_unlock(&g.lock);
    int codes[NXBSD_PIPELINE_MAX];
    for(size_t i=0;i<started;i++) {
        int rc=nxbsd_task_wait(&s,ids[i],&codes[i],0);
        if(rc && !e) e=errno;
    }
    if(!e) for(size_t i=0;i<n;i++) status[i]=codes[i];
    pthread_cond_destroy(&g.cv);
cleanup_mutex:
    pthread_mutex_destroy(&g.lock);
cleanup_scheduler:
    nxbsd_scheduler_destroy(&s);
cleanup_context:
    nxbsd_context_destroy(master);
    free(master); free(stage);
    return e ? bad(e) : 0;
}
