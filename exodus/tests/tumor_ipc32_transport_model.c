/* Host-only integrated IPC32/TMR1 model.
 * Separate backing regions for frame, publication, acknowledgement.
 * No Tegra MMIO, physical cache operations, or hardware admission.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ipc_native.c"

enum { OK=1, IDLE=0, FAULT=-1 };
typedef struct {
    uint8_t backing[24], tx_cache[24], rx_cache[24];
    uint32_t pub, ack, tx_ack, rx_pub;
    unsigned pending;
} lane;
typedef struct { uint64_t gen, seq; uint32_t token; uint8_t role, closed; } tx;
typedef struct { tmr_peer peer; uint32_t token; } rx;

/* The adapter contract: finish data clean before publishing token;
 * refresh token then invalidate data before read; ACK only after read.
 * All copies model completed maintenance, not physical instructions.
 */
static int send_model(lane *l,tx *t) {
    if(t->closed)return FAULT;
    l->tx_ack=l->ack;
    if(l->pub!=t->token || (uint32_t)(t->token-l->tx_ack)>1 ||
       t->seq==UINT64_MAX){t->closed=1;return FAULT;}
    if(t->token!=l->tx_ack)return IDLE;
    if(!tmr_encode(l->tx_cache,24,t->role,t->gen,t->seq+1)){
        t->closed=1;return FAULT;
    }
    l->pending=1;
    memcpy(l->backing,l->tx_cache,24); /* data clean completion */
    l->pending=0;
    assert(!l->pending);
    ++t->seq;
    l->pub=++t->token;
    return OK;
}
static int recv_model(lane *l,rx *r,uint64_t now) {
    if(!tmr_tick(&r->peer,now))return FAULT;
    l->rx_pub=l->pub;
    if(l->ack!=r->token || (uint32_t)(l->rx_pub-r->token)>1){
        r->peer.closed=1;return FAULT;
    }
    if(l->rx_pub==r->token)return IDLE;
    memcpy(l->rx_cache,l->backing,24); /* invalidate and read */
    if(!tmr_receive(&r->peer,l->rx_cache,24,now))return FAULT;
    r->token=l->rx_pub;
    l->ack=r->token;
    return OK;
}
static void setup(lane *l,tx *t,rx *r,uint8_t role,uint64_t gen,uint64_t prefix){
    memset(l,0,sizeof(*l));
    *t=(tx){.gen=gen,.seq=prefix,.token=(uint32_t)prefix,.role=role};
    assert(tmr_init(&r->peer,role,gen,0));
    r->peer.sequence=prefix;r->token=(uint32_t)prefix;
    l->pub=l->ack=t->token;
}
static void exchange(uint64_t prefix){
    lane l[2];tx t[2];rx r[2];
    setup(&l[0],&t[0],&r[0],1,UINT64_C(0x123400000008),prefix);
    setup(&l[1],&t[1],&r[1],2,UINT64_C(0x567800000009),prefix);
    for(unsigned i=0;i<10000;i++){
        unsigned first=i&1u,second=first^1u;
        assert(send_model(&l[first],&t[first])==OK);
        assert(send_model(&l[second],&t[second])==OK);
        assert(send_model(&l[first],&t[first])==IDLE);
        assert(recv_model(&l[second],&r[second],i+1)==OK);
        assert(recv_model(&l[first],&r[first],i+1)==OK);
        assert(recv_model(&l[first],&r[first],i+1)==IDLE);
    }
    for(unsigned i=0;i<2;i++){
        assert(t[i].seq==prefix+10000 && r[i].peer.sequence==t[i].seq);
        assert(l[i].pub==l[i].ack && l[i].ack==(uint32_t)t[i].seq);
    }
}
int main(void){
    exchange(0);exchange(UINT32_MAX-5000);
    puts("PASS: integrated IPC32 TMR1 model 40000 bidirectional frames");
    lane l;tx t;rx r;
    setup(&l,&t,&r,1,8,0);
    assert(tmr_encode(l.tx_cache,24,1,8,1));
    l.pub=1; /* notification before clean: actual TMR1 must reject */
    assert(recv_model(&l,&r,1)==FAULT && l.ack==0);
    assert(recv_model(&l,&r,2)==FAULT);
    setup(&l,&t,&r,1,8,0);
    assert(send_model(&l,&t)==OK);
    assert(tmr_encode(l.backing,24,1,7,1));
    assert(recv_model(&l,&r,1)==FAULT && l.ack==0);
    setup(&l,&t,&r,1,8,0);
    assert(send_model(&l,&t)==OK);
    l.backing[23]^=1;
    assert(recv_model(&l,&r,1)==FAULT && l.ack==0);
    setup(&l,&t,&r,1,8,0);
    assert(send_model(&l,&t)==OK);
    assert(recv_model(&l,&r,3000)==FAULT && l.ack==0);
    puts("PASS: integrated TMR1 early publish, stale generation, bad sequence, timeout");
    return 0;
}
