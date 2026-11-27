/* Reference platform contract, deliberately not a Tegra implementation.
 * All callbacks report completion, not merely issuance. A false return
 * closes the endpoint. Recovery requires external quiescence/new mapping.
 */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ipc_native.c"
enum { FRAME=0, PUB=1, ACK=2 };
enum { OK=1, IDLE=0, FAULT=-1 };
typedef struct {
    void *ctx;
    int (*write)(void*,unsigned,const uint8_t*,size_t);
    int (*read)(void*,unsigned,uint8_t*,size_t);
    int (*clean)(void*,unsigned);
    int (*invalidate)(void*,unsigned);
    int (*complete)(void*);
} platform;
typedef struct { uint64_t gen,seq;uint32_t token;uint8_t role,closed; } sender;
typedef struct { tmr_peer peer;uint32_t token; } receiver;
static int supported(const platform *p){
    return p&&p->write&&p->read&&p->clean&&p->invalidate&&p->complete;
}
static void encode32(uint8_t *b,uint32_t n){
    for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(n>>(8*i));
}
static uint32_t decode32(const uint8_t *b){
    uint32_t n=0;for(unsigned i=0;i<4;i++)n|=(uint32_t)b[i]<<(8*i);return n;
}
static int refresh(platform *p,unsigned region,uint8_t *b,size_t n){
    return p->invalidate(p->ctx,region)&&p->complete(p->ctx)&&p->read(p->ctx,region,b,n);
}
static int commit(platform *p,unsigned region,const uint8_t *b,size_t n){
    return p->write(p->ctx,region,b,n)&&p->clean(p->ctx,region)&&p->complete(p->ctx);
}
static int send_frame(platform *p,sender *s){
    uint8_t b[24],a[4];
    if(s->closed||!supported(p)||!refresh(p,ACK,a,4))goto fail;
    uint32_t ack=decode32(a);
    if((uint32_t)(s->token-ack)>1||s->seq==UINT64_MAX)goto fail;
    if(ack!=s->token)return IDLE;
    if(!tmr_encode(b,24,s->role,s->gen,s->seq+1)||!commit(p,FRAME,b,24))goto fail;
    encode32(a,s->token+1);
    if(!commit(p,PUB,a,4))goto fail;
    ++s->token;++s->seq;return OK;
fail:s->closed=1;return FAULT;
}
static int receive_frame(platform *p,receiver *r,uint64_t now){
    uint8_t a[4],b[24];
    if(!supported(p)||!tmr_tick(&r->peer,now)||!refresh(p,PUB,a,4))goto fail;
    uint32_t pub=decode32(a);
    if((uint32_t)(pub-r->token)>1)goto fail;
    if(pub==r->token)return IDLE;
    if(!refresh(p,FRAME,b,24)||!tmr_receive(&r->peer,b,24,now))goto fail;
    if(!commit(p,ACK,a,4))goto fail;
    r->token=pub;return OK;
fail:r->peer.closed=1;return FAULT;
}
/* Delayed clean mock: a write is not visible until complete succeeds.
 * A failed callback may leave ambiguous state; endpoints must not retry.
 */
typedef struct {
    uint8_t backing[3][24],cache[3][24];
    unsigned dirty,invalid,calls,fail_at,owner;
} mock;
static int step(mock *m){return ++m->calls!=m->fail_at;}
static size_t size(unsigned r){return r==FRAME?24:4;}
static int wr(void *v,unsigned r,const uint8_t *b,size_t n){
    mock *m=v;
    assert(r<3&&n==size(r)&&((m->owner==0&&r!=ACK)||(m->owner==1&&r==ACK)));
    if(!step(m))return 0;
    memcpy(m->cache[r],b,n);return 1;
}
static int rd(void *v,unsigned r,uint8_t *b,size_t n){
    mock *m=v;assert(r<3&&n==size(r));
    if(!step(m))return 0;
    memcpy(b,m->cache[r],n);return 1;
}
static int cl(void *v,unsigned r){
    mock *m=v;if(!step(m))return 0;m->dirty|=1u<<r;return 1;
}
static int inv(void *v,unsigned r){
    mock *m=v;if(!step(m))return 0;m->invalid|=1u<<r;return 1;
}
static int done(void *v){
    mock *m=v;if(!step(m))return 0;
    for(unsigned i=0;i<3;i++){
        if(m->dirty&(1u<<i))memcpy(m->backing[i],m->cache[i],size(i));
        if(m->invalid&(1u<<i))memcpy(m->cache[i],m->backing[i],size(i));
    }
    m->dirty=m->invalid=0;return 1;
}
static platform bind(mock *m){return(platform){m,wr,rd,cl,inv,done};}
static void fresh(mock *m,sender *s,receiver *r){
    memset(m,0,sizeof(*m));*s=(sender){.gen=8,.role=1};
    assert(tmr_init(&r->peer,1,8,0));r->token=0;
}
int main(void){
    mock m;sender s;receiver r;platform p=bind(&m);
    fresh(&m,&s,&r);
    for(unsigned i=0;i<10000;i++){
        m.owner=0;assert(send_frame(&p,&s)==OK);assert(send_frame(&p,&s)==IDLE);
        m.owner=1;assert(receive_frame(&p,&r,i+1)==OK);
        assert(receive_frame(&p,&r,i+1)==IDLE);
    }
    assert(s.seq==10000&&r.peer.sequence==10000);
    puts("PASS: platform callback contract 10000 exchanges");
    /* Each successful operation invokes nine callbacks. Inject failure at
     * every point; no endpoint is allowed to retry after ambiguous state.
     */
    for(unsigned i=1;i<=9;i++){
        fresh(&m,&s,&r);m.fail_at=i;
        assert(send_frame(&p,&s)==FAULT&&s.closed);
        assert(decode32(m.backing[PUB])==0);
        unsigned n=m.calls;assert(send_frame(&p,&s)==FAULT&&m.calls==n);
        fresh(&m,&s,&r);assert(send_frame(&p,&s)==OK);
        m.owner=1;m.calls=0;m.fail_at=i;
        assert(receive_frame(&p,&r,1)==FAULT&&r.peer.closed);
        assert(decode32(m.backing[ACK])==0);
        n=m.calls;assert(receive_frame(&p,&r,2)==FAULT&&m.calls==n);
    }
    puts("PASS: 18 platform callback fault injections close endpoints");
    fresh(&m,&s,&r);p.complete=NULL;
    assert(send_frame(&p,&s)==FAULT&&receive_frame(&p,&r,1)==FAULT&&m.calls==0);
    puts("PASS: incomplete platform adapter denied");
    return 0;
}
