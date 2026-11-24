#include "nxbsd_task.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
static int bad(int e) { errno=e; return -1; }
static struct nxbsd_task *lookup(struct nxbsd_scheduler *s, struct nxbsd_task_id id)
{
    if (id.slot>=NXBSD_TASK_MAX) return NULL;
    struct nxbsd_task *t=&s->tasks[id.slot];
    return t->state!=NXBSD_TASK_FREE && t->generation==id.generation ? t : NULL;
}
static void *worker(void *arg)
{
    struct nxbsd_task *t=arg;
    int status=t->fn(t->ctx,t->arg);
    nxbsd_context_destroy(t->ctx);
    free(t->ctx);
    pthread_mutex_lock(&t->owner->lock);
    t->ctx=NULL;
    t->status=status;
    if(t->state==NXBSD_TASK_RUNNING) t->state=NXBSD_TASK_EXITED;
    pthread_mutex_unlock(&t->owner->lock);
    return NULL;
}
int nxbsd_scheduler_init(struct nxbsd_scheduler *s)
{
    if(!s) return bad(EINVAL);
    memset(s,0,sizeof(*s));
    int e=pthread_mutex_init(&s->lock,NULL);
    return e?bad(e):0;
}
int nxbsd_scheduler_destroy(struct nxbsd_scheduler *s)
{
    if(!s) return bad(EINVAL);
    pthread_mutex_lock(&s->lock);
    for(unsigned i=0;i<NXBSD_TASK_MAX;i++)
        if(s->tasks[i].state!=NXBSD_TASK_FREE) {
            pthread_mutex_unlock(&s->lock);
            return bad(EBUSY);
        }
    pthread_mutex_unlock(&s->lock);
    int e=pthread_mutex_destroy(&s->lock);
    return e?bad(e):0;
}
int nxbsd_task_spawn(struct nxbsd_scheduler *s,const struct nxbsd_context *parent,
                    nxbsd_task_fn fn,void *arg,struct nxbsd_task_id *out)
{
    if(!s || !parent || !fn || !out) return bad(EINVAL);
    pthread_mutex_lock(&s->lock);
    unsigned i;
    for(i=0;i<NXBSD_TASK_MAX;i++)
        if(s->tasks[i].state==NXBSD_TASK_FREE && s->tasks[i].generation!=UINT64_MAX) break;
    if(i==NXBSD_TASK_MAX) { pthread_mutex_unlock(&s->lock); return bad(EAGAIN); }
    struct nxbsd_context *ctx=malloc(sizeof(*ctx));
    if(!ctx) { pthread_mutex_unlock(&s->lock); return bad(ENOMEM); }
    if(nxbsd_context_clone(ctx,parent)) {
        int e=errno; free(ctx); pthread_mutex_unlock(&s->lock); return bad(e);
    }
    struct nxbsd_task *t=&s->tasks[i];
    t->ctx=ctx; t->fn=fn; t->arg=arg; t->owner=s; t->status=0;
    t->state=NXBSD_TASK_RUNNING;
    int e=pthread_create(&t->thread,NULL,worker,t);
    if(e) {
        nxbsd_context_destroy(ctx); free(ctx); t->ctx=NULL;
        t->state=NXBSD_TASK_FREE; pthread_mutex_unlock(&s->lock); return bad(e);
    }
    ++t->generation;
    *out=(struct nxbsd_task_id){i,t->generation};
    pthread_mutex_unlock(&s->lock);
    return 0;
}
int nxbsd_task_poll(struct nxbsd_scheduler *s,struct nxbsd_task_id id,int *exited)
{
    if(!s || !exited) return bad(EINVAL);
    pthread_mutex_lock(&s->lock);
    struct nxbsd_task *t=lookup(s,id);
    if(!t || t->state==NXBSD_TASK_JOINING) {
        int e=t?EBUSY:ECHILD; pthread_mutex_unlock(&s->lock); return bad(e);
    }
    *exited=t->state==NXBSD_TASK_EXITED;
    pthread_mutex_unlock(&s->lock);
    return 0;
}
int nxbsd_task_wait(struct nxbsd_scheduler *s,struct nxbsd_task_id id,int *status,int nonblock)
{
    if(!s || !status) return bad(EINVAL);
    pthread_mutex_lock(&s->lock);
    struct nxbsd_task *t=lookup(s,id);
    if(!t || t->state==NXBSD_TASK_JOINING) {
        int e=t?EBUSY:ECHILD; pthread_mutex_unlock(&s->lock); return bad(e);
    }
    if(pthread_equal(pthread_self(),t->thread)) {
        pthread_mutex_unlock(&s->lock); return bad(EDEADLK);
    }
    if(nonblock && t->state!=NXBSD_TASK_EXITED) {
        pthread_mutex_unlock(&s->lock); return bad(EAGAIN);
    }
    t->state=NXBSD_TASK_JOINING;
    pthread_t th=t->thread;
    pthread_mutex_unlock(&s->lock);
    int e=pthread_join(th,NULL);
    pthread_mutex_lock(&s->lock);
    if(e) {
        t->state=t->ctx?NXBSD_TASK_RUNNING:NXBSD_TASK_EXITED;
        pthread_mutex_unlock(&s->lock); return bad(e);
    }
    *status=t->status;
    t->fn=NULL; t->arg=NULL; t->owner=NULL;
    t->state=NXBSD_TASK_FREE;
    pthread_mutex_unlock(&s->lock);
    return 0;
}
