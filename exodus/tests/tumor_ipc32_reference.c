/* IPC32 reference: preassigned SPSC endpoints on coherent host memory.
 * Noncoherent ADSP mappings require a separately verified platform adapter.
 * A mediator must establish ownership and quiescence outside this protocol.
 * No compare-exchange, shared read-modify-write, device access or live reset.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>

enum { IPC_FAULT=-1, IPC_IDLE=0, IPC_OK=1 };
typedef struct {
    _Alignas(64) _Atomic uint32_t token;
    uint8_t bytes[24];
} producer_line;
typedef struct { _Alignas(64) _Atomic uint32_t token; } consumer_line;
typedef struct { producer_line pub; consumer_line ack; } ipc32_slot;
typedef struct { uint32_t token; uint64_t sequence, generation; uint8_t role, closed; } endpoint;
_Static_assert(sizeof(producer_line)==64 && sizeof(consumer_line)==64,"separate ownership");
_Static_assert(offsetof(ipc32_slot,ack)==64 && sizeof(ipc32_slot)==128,"layout");
_Static_assert(sizeof(_Atomic uint32_t)==4,"atomic width");

static int init(ipc32_slot *s) {
    atomic_init(&s->pub.token,0); atomic_init(&s->ack.token,0);
    memset(s->pub.bytes,0,sizeof(s->pub.bytes));
    return atomic_is_lock_free(&s->pub.token) && atomic_is_lock_free(&s->ack.token);
}
static void put64(uint8_t *p,uint64_t v) {
    for(unsigned i=8;i;){p[--i]=(uint8_t)v;v>>=8;}
}
static uint64_t get64(const uint8_t *p) {
    uint64_t v=0;for(unsigned i=0;i<8;i++)v=(v<<8)|p[i];return v;
}
static int fault(endpoint *e){e->closed=1;return IPC_FAULT;}
static int valid(const endpoint *e){return !e->closed && e->generation && (e->role==1||e->role==2);}
static int send(ipc32_slot *s,endpoint *e){
    if(!valid(e))return fault(e);
    uint32_t pub=atomic_load_explicit(&s->pub.token,memory_order_relaxed);
    uint32_t ack=atomic_load_explicit(&s->ack.token,memory_order_acquire);
    if(pub!=e->token || (uint32_t)(pub-ack)>1 || e->sequence==UINT64_MAX)return fault(e);
    if(pub!=ack)return IPC_IDLE;
    uint8_t b[24]={'T','M','R','1',1,e->role,0,0};
    put64(b+8,e->generation);put64(b+16,++e->sequence);
    memcpy(s->pub.bytes,b,sizeof(b));
    ++e->token;
    atomic_store_explicit(&s->pub.token,e->token,memory_order_release);
    return IPC_OK;
}
static int receive(ipc32_slot *s,endpoint *e){
    if(!valid(e))return fault(e);
    uint32_t ack=atomic_load_explicit(&s->ack.token,memory_order_relaxed);
    uint32_t pub=atomic_load_explicit(&s->pub.token,memory_order_acquire);
    if(ack!=e->token || (uint32_t)(pub-ack)>1)return fault(e);
    if(pub==ack)return IPC_IDLE;
    uint8_t b[24];memcpy(b,s->pub.bytes,sizeof(b));
    const uint8_t head[8]={'T','M','R','1',1,e->role,0,0};
    if(memcmp(b,head,8)||get64(b+8)!=e->generation||
       e->sequence==UINT64_MAX||get64(b+16)!=e->sequence+1)return fault(e);
    ++e->sequence;e->token=pub;
    atomic_store_explicit(&s->ack.token,pub,memory_order_release);
    return IPC_OK;
}
static void setup(ipc32_slot *s,endpoint *tx,endpoint *rx,uint64_t gen){
    assert(init(s));*tx=*rx=(endpoint){.generation=gen,.role=1};
}
enum { STRESS_COUNT=100000 };
typedef struct {
    ipc32_slot *out,*in;
    endpoint tx,rx;
    uint64_t end_sequence;
    int result;
} worker;
static uint64_t monotonic_ms(void){
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC,&ts)==0);
    return (uint64_t)ts.tv_sec*1000+(uint64_t)ts.tv_nsec/1000000;
}
static void *exchange(void *arg){
    worker *w=arg;
    uint64_t start=monotonic_ms();
    w->result=1;
    for(;;){
        int a=IPC_IDLE,b=IPC_IDLE;
        if(w->tx.sequence<w->end_sequence)a=send(w->out,&w->tx);
        if(w->rx.sequence<w->end_sequence)b=receive(w->in,&w->rx);
        if(a==IPC_FAULT||b==IPC_FAULT)return NULL;
        if(w->tx.sequence==w->end_sequence &&
           w->rx.sequence==w->end_sequence &&
           atomic_load_explicit(&w->out->ack.token,memory_order_acquire)==w->tx.token){
            w->result=0;return NULL;
        }
        if(monotonic_ms()-start>=20000)return NULL;
        if(a==IPC_IDLE&&b==IPC_IDLE)sched_yield();
    }
}
static void stress(uint64_t prefix){
    ipc32_slot lanes[2];
    const uint64_t ga=UINT64_C(0x123400000001),gb=UINT64_C(0x567800000002);
    const uint32_t token=(uint32_t)prefix;
    for(unsigned i=0;i<2;i++){
        assert(init(&lanes[i]));
        atomic_store(&lanes[i].pub.token,token);
        atomic_store(&lanes[i].ack.token,token);
    }
    worker a={.out=&lanes[0],.in=&lanes[1],
        .tx={token,prefix,ga,1,0},.rx={token,prefix,gb,2,0},
        .end_sequence=prefix+STRESS_COUNT,.result=1};
    worker b={.out=&lanes[1],.in=&lanes[0],
        .tx={token,prefix,gb,2,0},.rx={token,prefix,ga,1,0},
        .end_sequence=prefix+STRESS_COUNT,.result=1};
    pthread_t pa,pb;
    assert(pthread_create(&pa,NULL,exchange,&a)==0);
    assert(pthread_create(&pb,NULL,exchange,&b)==0);
    assert(pthread_join(pa,NULL)==0);
    assert(pthread_join(pb,NULL)==0);
    assert(a.result==0&&b.result==0);
    assert(a.tx.sequence==a.end_sequence&&a.rx.sequence==a.end_sequence);
    assert(b.tx.sequence==b.end_sequence&&b.rx.sequence==b.end_sequence);
    for(unsigned i=0;i<2;i++){
        assert(atomic_load(&lanes[i].pub.token)==(uint32_t)a.end_sequence);
        assert(atomic_load(&lanes[i].ack.token)==(uint32_t)a.end_sequence);
    }
}
int main(void){
    ipc32_slot s;endpoint tx,rx;
    setup(&s,&tx,&rx,UINT64_C(0x100000007));
    assert(receive(&s,&rx)==IPC_IDLE);
    for(unsigned i=0;i<1000;i++){
        assert(send(&s,&tx)==IPC_OK);
        assert(send(&s,&tx)==IPC_IDLE);
        assert(receive(&s,&rx)==IPC_OK);
        assert(receive(&s,&rx)==IPC_IDLE);
    }
    assert(rx.sequence==1000);
    puts("PASS: IPC32 ownership, delivery and backpressure");
    setup(&s,&tx,&rx,7);
    tx.token=rx.token=UINT32_MAX-1;
    tx.sequence=rx.sequence=UINT32_MAX-1;
    atomic_store(&s.pub.token,tx.token);atomic_store(&s.ack.token,rx.token);
    for(unsigned i=0;i<3;i++){assert(send(&s,&tx)==IPC_OK);assert(receive(&s,&rx)==IPC_OK);}
    assert(rx.token==1 && rx.sequence==UINT64_C(0x100000001));
    puts("PASS: IPC32 token wrap with full-width sequence");
    setup(&s,&tx,&rx,7);rx.generation=8;
    assert(send(&s,&tx)==IPC_OK);assert(receive(&s,&rx)==IPC_FAULT);
    assert(atomic_load(&s.ack.token)==0 && receive(&s,&rx)==IPC_FAULT);
    setup(&s,&tx,&rx,8);
    atomic_store(&s.pub.token,2);assert(receive(&s,&rx)==IPC_FAULT);
    assert(atomic_load(&s.ack.token)==0);
    setup(&s,&tx,&rx,8);
    assert(send(&s,&tx)==IPC_OK);s.pub.bytes[0]^=1;
    assert(receive(&s,&rx)==IPC_FAULT && atomic_load(&s.ack.token)==0);
    puts("PASS: IPC32 stale generation, invalid counter and corrupt frame fail closed");
    setup(&s,&tx,&rx,9);tx.sequence=UINT64_MAX;
    assert(send(&s,&tx)==IPC_FAULT && atomic_load(&s.pub.token)==0);
    puts("PASS: IPC32 sequence exhaustion requires fresh session");
    stress(0);
    puts("PASS: IPC32 concurrent bidirectional 100000 frames per direction");
    stress(UINT64_C(0xffffffff)-50000);
    puts("PASS: IPC32 concurrent bidirectional 100000 frames per direction across token wrap");
    return 0;
}
