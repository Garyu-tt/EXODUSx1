/* Remote wait reference: bounded WNOHANG, exact-child only, no pointers.
 * Caller serializes access; native kernel adapter must authenticate owner. */
#ifndef EXODUS_WAIT_CONTRACT_H
#define EXODUS_WAIT_CONTRACT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum { EW_SIZE=64, EW_REQUEST=1, EW_REPLY=2, EW_NOHANG=1,
       EW_OK=0, EW_ECHILD=10, EW_EINVAL=22 };
enum { EW_EMPTY=0, EW_LIVE=1, EW_ZOMBIE=2, EW_REAPED=3 };
struct ew_frame {
    uint64_t generation, seq, parent, child;
    uint32_t pid, wait_status;
    uint8_t kind, options, error;
};
struct ew_child {
    uint64_t task,parent;
    uint32_t pid,status;
    uint8_t state;
};
struct ew_parent {
    uint64_t generation, task, last;
};
static inline void ew_put(uint8_t *p,uint64_t v,unsigned n){
    for(unsigned i=0;i<n;i++){p[i]=(uint8_t)v;v>>=8;}
}
static inline uint64_t ew_get(const uint8_t *p,unsigned n){
    uint64_t v=0;while(n)v=(v<<8)|p[--n];return v;
}
static inline int ew_status_valid(uint32_t s){
    /* Only normal exit or terminal signal, not stopped/continued/core flags. */
    return s<=0xff00u && ((s&255u)==0 || (s>=1&&s<=126));
}
static inline int ew_valid(const struct ew_frame *f){
    if(!f->generation||!f->seq||!f->parent||!f->child||f->parent==f->child)return 0;
    if(f->options!=EW_NOHANG)return 0;
    if(f->kind==EW_REQUEST)return !f->pid&&!f->wait_status&&!f->error;
    if(f->kind!=EW_REPLY)return 0;
    if(f->error)return (f->error==EW_ECHILD||f->error==EW_EINVAL)&&!f->pid&&!f->wait_status;
    if(!f->pid)return !f->wait_status;
    return f->pid<=0x7fffffffu&&ew_status_valid(f->wait_status);
}
static inline int ew_encode(uint8_t out[EW_SIZE],const struct ew_frame *f){
    if(!out||!f||!ew_valid(f))return 0;
    memset(out,0,EW_SIZE);memcpy(out,"AWT1",4);
    out[4]=1;out[5]=f->kind;out[6]=f->options;out[7]=f->error;
    ew_put(out+8,f->generation,8);ew_put(out+16,f->seq,8);
    ew_put(out+24,f->parent,8);ew_put(out+32,f->child,8);
    ew_put(out+40,f->pid,4);ew_put(out+44,f->wait_status,4);
    return 1;
}
static inline int ew_decode(struct ew_frame *f,const uint8_t *in,size_t n){
    if(!f||!in||n!=EW_SIZE||memcmp(in,"AWT1",4)||in[4]!=1)return 0;
    for(unsigned i=48;i<EW_SIZE;i++)if(in[i])return 0;
    struct ew_frame t={.generation=ew_get(in+8,8),.seq=ew_get(in+16,8),
        .parent=ew_get(in+24,8),.child=ew_get(in+32,8),
        .pid=(uint32_t)ew_get(in+40,4),.wait_status=(uint32_t)ew_get(in+44,4),
        .kind=in[5],.options=in[6],.error=in[7]};
    if(!ew_valid(&t))return 0;
    *f=t;return 1;
}
static inline int ew_register(struct ew_child *c,uint64_t task,uint64_t parent,uint32_t pid){
    if(!c||c->state!=EW_EMPTY||!task||!parent||task==parent||!pid||pid>0x7fffffffu)return 0;
    *c=(struct ew_child){.task=task,.parent=parent,.pid=pid,.state=EW_LIVE};return 1;
}
/* Kernel event adapter only: caller must supply an authenticated native exit. */
static inline int ew_exit(struct ew_child *c,uint32_t status){
    if(!c||c->state!=EW_LIVE||!ew_status_valid(status))return 0;
    c->status=status;c->state=EW_ZOMBIE;return 1;
}
/* Return 0 on transport/authentication rejection (no state mutation).
 * Valid requests including ECHILD consume sequence. Live child returns 0 PID.
 * Exact matching zombie is reaped once. Output failure policy is fail-closed:
 * caller must reserve the response buffer before calling. */
static inline int ew_serve(struct ew_parent *p,struct ew_child *table,size_t count,
        const uint8_t *q,size_t len,uint8_t out[EW_SIZE]){
    struct ew_frame f;
    if(!p||!out||(!table&&count)||count>64||!ew_decode(&f,q,len)||
       f.kind!=EW_REQUEST||f.generation!=p->generation||f.parent!=p->task||
       p->last==UINT64_MAX||f.seq!=p->last+1)return 0;
    struct ew_child *found=NULL;
    for(size_t i=0;i<count;i++)if(table[i].state!=EW_EMPTY&&table[i].task==f.child){
        if(found)return 0;
        found=&table[i];
    }
    f.kind=EW_REPLY;
    if(!found||found->parent!=p->task||found->state==EW_REAPED)f.error=EW_ECHILD;
    else if(found->state==EW_ZOMBIE){f.pid=found->pid;f.wait_status=found->status;}
    else if(found->state!=EW_LIVE)return 0;
    if(!ew_encode(out,&f))return 0;
    p->last=f.seq;
    if(f.pid)found->state=EW_REAPED;
    return 1;
}
static inline int ew_match(const struct ew_frame *q,const uint8_t *in,size_t len,struct ew_frame *out){
    struct ew_frame r;
    if(!q||!out||q->kind!=EW_REQUEST||!ew_valid(q)||!ew_decode(&r,in,len)||
       r.kind!=EW_REPLY||r.generation!=q->generation||r.seq!=q->seq||
       r.parent!=q->parent||r.child!=q->child||r.options!=q->options)return 0;
    *out=r;return 1;
}
#endif
