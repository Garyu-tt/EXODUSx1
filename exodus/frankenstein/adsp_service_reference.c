/* EXODUS-owned reference lifecycle only; NOT the Nintendo firmware ABI.
 * Single serialized caller, no MMIO, DMA, audio processing or physical admission.
 * Completion represents a synthetic backend event, not an audio delivery proof. */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
enum state { READY, RUNNING, SUSPENDED, FAULT };
enum result { OK, INVALID, BUSY, STALE, TIMEOUT, FAILED };
struct service {
    enum state state;
    uint64_t generation, last, pending, deadline, now;
    uint32_t pool;
};
static enum result fail(struct service *s, enum result r) {
    s->state=FAULT; s->pending=0; s->deadline=0; return r;
}
static enum result tick(struct service *s, uint64_t now) {
    if(s->state==FAULT) return FAILED;
    if(now<s->now) return fail(s,INVALID);
    s->now=now;
    if(s->pending && now>=s->deadline) return fail(s,TIMEOUT);
    return OK;
}
static enum result init(struct service *s, uint64_t gen, uint32_t pool) {
    *s=(struct service){.state=FAULT};
    if(!gen || pool<4 || (pool%4)) return INVALID;
    *s=(struct service){.state=READY,.generation=gen,.pool=pool};
    return OK;
}
static enum result transition(struct service *s, uint64_t gen,
    enum state expected, enum state next, uint64_t now) {
    if(gen!=s->generation) return STALE;
    enum result r=tick(s,now);
    if(r!=OK) return r;
    if(s->state!=expected) return INVALID;
    if(s->pending) return BUSY;
    if(!((expected==READY && next==RUNNING) ||
         (expected==RUNNING && next==READY) ||
         (expected==READY && next==SUSPENDED) ||
         (expected==SUSPENDED && next==READY))) return INVALID;
    s->state=next; return OK;
}
static enum result submit(struct service *s, uint64_t gen, uint64_t id,
    uint32_t offset, uint32_t len, uint64_t now, uint64_t budget) {
    if(gen!=s->generation) return STALE;
    enum result r=tick(s,now);
    if(r!=OK) return r;
    if(s->state!=RUNNING) return INVALID;
    if(s->pending) return BUSY;
    if(s->last==UINT64_MAX || id!=s->last+1 || !len || (offset%4) ||
       (len%4) || offset>s->pool || len>s->pool-offset ||
       !budget || budget>UINT64_MAX-now) return INVALID;
    s->pending=id; s->deadline=now+budget; return OK;
}
static enum result complete(struct service *s, uint64_t gen, uint64_t id,
    uint64_t now, int backend_ok) {
    if(gen!=s->generation) return STALE;
    enum result r=tick(s,now);
    if(r!=OK) return r;
    if(!s->pending || id!=s->pending) return STALE;
    if(!backend_ok) return fail(s,FAILED);
    s->last=id; s->pending=0; s->deadline=0; return OK;
}
int main(void) {
    struct service s;
    const uint64_t g=UINT64_C(0x1020304050607080);
    assert(init(&s,0,4096)==INVALID);
    assert(init(&s,g,4095)==INVALID);
    assert(init(&s,g,4096)==OK);
    assert(submit(&s,g,1,0,4,0,1)==INVALID);
    assert(transition(&s,g,READY,RUNNING,0)==OK);
    assert(transition(&s,g,RUNNING,SUSPENDED,0)==INVALID);
    assert(submit(&s,g,1,4092,8,1,10)==INVALID);
    assert(submit(&s,g,1,UINT32_MAX-3,8,1,10)==INVALID);
    assert(submit(&s,g,1,0,0,1,10)==INVALID);
    assert(submit(&s,g,1,1,4,1,10)==INVALID);
    assert(submit(&s,g,1,0,4,1,0)==INVALID);
    for(uint64_t i=1;i<=128;i++) {
        assert(submit(&s,g,i,4092,4,2*i,10)==OK);
        assert(submit(&s,g,i+1,0,4,2*i,10)==BUSY);
        assert(transition(&s,g,RUNNING,READY,2*i)==BUSY);
        assert(complete(&s,g+1,i,2*i+1,1)==STALE);
        assert(complete(&s,g,i+1,2*i+1,1)==STALE);
        assert(complete(&s,g,i,2*i+1,1)==OK);
        assert(complete(&s,g,i,2*i+1,1)==STALE);
    }
    assert(s.last==128);
    assert(transition(&s,g,RUNNING,READY,258)==OK);
    assert(transition(&s,g,READY,SUSPENDED,259)==OK);
    assert(submit(&s,g,129,0,4,259,10)==INVALID);
    assert(transition(&s,g,SUSPENDED,READY,260)==OK);
    assert(transition(&s,g,READY,RUNNING,261)==OK);
    assert(submit(&s,g,129,0,4,262,10)==OK);
    assert(complete(&s,g,129,272,1)==TIMEOUT);
    assert(s.state==FAULT && s.last==128 && !s.pending);
    assert(transition(&s,g,FAULT,READY,273)==FAILED);
    /* New, externally quiesced reference session, not a live reset. */
    assert(init(&s,g+1,4096)==OK);
    assert(transition(&s,g+1,READY,RUNNING,0)==OK);
    assert(submit(&s,g+1,1,0,4,1,1)==OK);
    assert(complete(&s,g+1,1,1,0)==FAILED && s.state==FAULT);
    assert(init(&s,g+2,4096)==OK);
    assert(transition(&s,g+2,READY,RUNNING,10)==OK);
    assert(tick(&s,9)==INVALID && s.state==FAULT);
    puts("PASS: synthetic ADSP lifecycle, 128 completions, bounds, stale events, drain, timeout, backend failure and monotonic clock");
    return 0;
}
