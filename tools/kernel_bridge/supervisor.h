#ifndef EXODUS_SUPERVISOR_H
#define EXODUS_SUPERVISOR_H
#include "process.h"
#include <string.h>
#define EXS_MAX 16
enum exs_state { EXS_WAIT, EXS_RUNNING, EXS_READY, EXS_BACKOFF, EXS_DONE, EXS_FAILED };
/* Dependency bits refer to indices in the immutable service manifest.
   A daemon requires explicit readiness; one-shot success satisfies dependencies. */
struct exs_spec {
    const char *name, *program;
    uint32_t requires;
    unsigned daemon, max_restarts;
    uint64_t backoff_ms;
};
struct exs_service {
    enum exs_state state;
    struct exp_token token;
    unsigned restarts;
    uint64_t retry_at;
    enum exp_status last_status;
    int native_error, exit_code, signaled;
};
struct exs {
    struct exp_manager *pm;
    const struct exs_spec *spec;
    unsigned count, initialized;
    uint64_t last_now;
    struct exs_service services[EXS_MAX];
};
static inline int exs_init(struct exs *s,struct exp_manager *pm,
                           const struct exs_spec *spec,unsigned n) {
    if (!s || s->initialized || !pm || !spec || !n || n>EXS_MAX) return -1;
    const uint32_t all=(UINT32_C(1)<<n)-1;
    for(unsigned i=0;i<n;++i) {
        if(!spec[i].name || !*spec[i].name || !spec[i].program || !*spec[i].program ||
           (spec[i].requires & ~all) || spec[i].daemon>1) return -1;
        for(unsigned j=0;j<i;++j) if(!strcmp(spec[i].name,spec[j].name)) return -1;
    }
    uint32_t sorted=0;
    for(unsigned round=0;round<n;++round)
        for(unsigned i=0;i<n;++i)
            if(!(spec[i].requires & ~sorted)) sorted|=UINT32_C(1)<<i;
    if(sorted!=all) return -1;
    *s=(struct exs){0};
    s->pm=pm; s->spec=spec; s->count=n; s->initialized=1;
    return 0;
}
static inline int exs_ready(struct exs *s,unsigned i,struct exp_token token) {
    if(!s || !s->initialized || i>=s->count || !s->spec[i].daemon) return -1;
    struct exs_service *v=&s->services[i];
    if(v->state!=EXS_RUNNING || v->token.slot!=token.slot ||
       v->token.generation!=token.generation) return -1;
    v->state=EXS_READY;
    return 0;
}
static inline void exs_retry(struct exs_service *v,const struct exs_spec *p,uint64_t now) {
    if(v->restarts>=p->max_restarts) { v->state=EXS_FAILED; return; }
    ++v->restarts;
    v->retry_at=p->backoff_ms>UINT64_MAX-now ? UINT64_MAX : now+p->backoff_ms;
    v->state=EXS_BACKOFF;
}
static inline int exs_tick(struct exs *s,uint64_t now) {
    if(!s || !s->initialized || now<s->last_now) return -1;
    s->last_now=now;
    /* Observe every owned process before admitting new launches. */
    for(unsigned i=0;i<s->count;++i) {
        struct exs_service *v=&s->services[i];
        if(v->state!=EXS_RUNNING && v->state!=EXS_READY) continue;
        struct exp_child c;
        v->last_status=exp_poll(s->pm,v->token,&c,&v->native_error);
        if(v->last_status!=EXP_OK) {
            /* Failed poll is unknown, never report a daemon ready on that basis. */
            if(v->last_status!=EXP_PENDING) v->state=EXS_RUNNING;
            continue;
        }
        if(exp_release(s->pm,v->token)!=EXP_OK) return -1;
        v->exit_code=c.code; v->signaled=c.signaled;
        if(!s->spec[i].daemon && !c.signaled && c.code==0) v->state=EXS_DONE;
        else exs_retry(v,&s->spec[i],now);
    }
    uint32_t ready=0;
    for(unsigned i=0;i<s->count;++i)
        if(s->services[i].state==EXS_READY || s->services[i].state==EXS_DONE)
            ready|=UINT32_C(1)<<i;
    for(unsigned i=0;i<s->count;++i) {
        struct exs_service *v=&s->services[i];
        const struct exs_spec *p=&s->spec[i];
        if(v->state!=EXS_WAIT && v->state!=EXS_BACKOFF) continue;
        if(v->state==EXS_BACKOFF && now<v->retry_at) continue;
        if(p->requires & ~ready) continue;
        v->last_status=exp_spawn(s->pm,p->program,&v->token,&v->native_error);
        if(v->last_status==EXP_OK) v->state=EXS_RUNNING;
        else if(v->last_status==EXP_FULL) continue;
        else if(v->last_status==EXP_UNSUPPORTED || v->last_status==EXP_INVALID)
            v->state=EXS_FAILED;
        else exs_retry(v,p,now);
    }
    return 0;
}
#endif
