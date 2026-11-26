#ifndef EXODUS_SPAWN_CONTRACT_H
#define EXODUS_SPAWN_CONTRACT_H
#include "amp_wait_contract.h"
/* Separate admission frame: same offsets, independent sequence space.
 * A request has zero PID/status; a reply must contain a positive PID.
 * Slot, generation and parent are validated by the kernel adapter. */
static inline int aws_encode(uint8_t out[EW_SIZE],const struct ew_frame *f){
    if(!f||f->wait_status||f->error||(f->kind==EW_REPLY&&!f->pid))return 0;
    if(!ew_encode(out,f))return 0;
    memcpy(out,"AWS1",4);return 1;
}
static inline int aws_decode(struct ew_frame *f,const uint8_t *in,size_t n){
    uint8_t tmp[EW_SIZE];struct ew_frame t;
    if(!f||!in||n!=EW_SIZE||memcmp(in,"AWS1",4))return 0;
    memcpy(tmp,in,n);memcpy(tmp,"AWT1",4);
    if(!ew_decode(&t,tmp,n)||t.wait_status||t.error||(t.kind==EW_REPLY&&!t.pid))return 0;
    *f=t;return 1;
}
static inline int aws_match(const struct ew_frame *q,const uint8_t *in,size_t n,struct ew_frame *out){
    struct ew_frame t;
    if(!q||!out||q->kind!=EW_REQUEST||!aws_decode(&t,in,n)||t.kind!=EW_REPLY||
       t.generation!=q->generation||t.seq!=q->seq||t.parent!=q->parent||
       t.child!=q->child||t.options!=q->options)return 0;
    *out=t;return 1;
}
#endif
