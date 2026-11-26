#ifndef EXODUS_PROCESS_H
#define EXODUS_PROCESS_H
#include <stdint.h>
#include <stddef.h>
#define EXP_MAX 16
enum exp_status { EXP_OK, EXP_PENDING, EXP_INVALID, EXP_FULL, EXP_NATIVE, EXP_UNSUPPORTED, EXP_BUSY };
struct exp_token { unsigned slot; uint64_t generation; };
struct exp_child { uint64_t identity; int exited, code, signaled; };
/* Spawn transfers exclusive child ownership on success. Poll is nonblocking,
   sets exited only after termination has been observed and reaped.
   On failure ownership remains with manager; backend must not lose child. */
struct exp_backend {
    void *ctx;
    int (*spawn)(void *, const char *, uint64_t *);
    int (*poll)(void *, uint64_t, struct exp_child *);
};
struct exp_entry { uint64_t generation; int occupied; struct exp_child child; };
struct exp_manager { struct exp_backend backend; struct exp_entry entries[EXP_MAX]; };
static inline struct exp_entry *exp_lookup(struct exp_manager *m, struct exp_token t) {
    if (!m || t.slot>=EXP_MAX) return NULL;
    struct exp_entry *e=&m->entries[t.slot];
    return e->occupied && e->generation==t.generation ? e : NULL;
}
static inline enum exp_status exp_spawn(struct exp_manager *m,const char *program,
                                       struct exp_token *token,int *native_error) {
    if (native_error) *native_error=0;
    if (!m || !program || !*program || !token || !native_error) return EXP_INVALID;
    if (!m->backend.spawn || !m->backend.poll) return EXP_UNSUPPORTED;
    unsigned i;
    for(i=0;i<EXP_MAX;++i)
        if(!m->entries[i].occupied && m->entries[i].generation!=UINT64_MAX) break;
    if(i==EXP_MAX) return EXP_FULL;
    uint64_t identity=0;
    *native_error=m->backend.spawn(m->backend.ctx,program,&identity);
    if(*native_error) return EXP_NATIVE;
    struct exp_entry *e=&m->entries[i];
    ++e->generation;
    e->occupied=1;
    e->child=(struct exp_child){identity,0,0,0};
    *token=(struct exp_token){i,e->generation};
    return EXP_OK;
}
static inline enum exp_status exp_poll(struct exp_manager *m,struct exp_token t,
                                       struct exp_child *out,int *native_error) {
    if(native_error) *native_error=0;
    struct exp_entry *e=exp_lookup(m,t);
    if(!e || !out || !native_error) return EXP_INVALID;
    if(!e->child.exited) {
        struct exp_child next=e->child;
        *native_error=m->backend.poll(m->backend.ctx,e->child.identity,&next);
        if(*native_error) return EXP_NATIVE;
        next.identity=e->child.identity;
        e->child=next;
    }
    *out=e->child;
    return e->child.exited ? EXP_OK : EXP_PENDING;
}
static inline enum exp_status exp_release(struct exp_manager *m,struct exp_token t) {
    struct exp_entry *e=exp_lookup(m,t);
    if(!e) return EXP_INVALID;
    if(!e->child.exited) return EXP_BUSY;
    e->occupied=0;
    return EXP_OK;
}
#endif
