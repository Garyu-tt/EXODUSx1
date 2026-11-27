/* R26 abstract mediator recovery policy, not Tegra power/reset control.
 * A retired mapping stays quarantined; the other lane never reuses it
 * before independently established quiescence and revocation.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "ipc_native.c"
enum phase { RUNNING,DRAINING,WAIT_QUIESCENT,RETIRED,BOOTING,WAIT_READY,ONLINE,QUARANTINED };
typedef struct { enum phase phase; uint64_t generation; unsigned in_flight,revoked,fresh,ready,reset_ok; } session;
static int request_stop(session *s){if(s->phase!=RUNNING)return 0;s->phase=DRAINING;return 1;}
static int submit(session *s){if(s->phase!=RUNNING||s->in_flight)return 0;s->in_flight=1;return 1;}
static int drain(session *s){if(s->phase!=DRAINING||s->in_flight)return 0;s->phase=WAIT_QUIESCENT;return 1;}
static int acknowledge(session *s){if((s->phase!=RUNNING&&s->phase!=DRAINING)||!s->in_flight)return 0;s->in_flight=0;return 1;}
static void fail(session *s){s->phase=QUARANTINED;}
static int retire(session *s,int quiescent,int revoked){
    if(s->phase!=WAIT_QUIESCENT||!quiescent||!revoked){fail(s);return 0;}
    s->revoked=1;s->phase=RETIRED;return 1;
}
static int boot(session *s,int distinct_mapping,uint64_t generation){
    if(s->phase!=RETIRED||!s->revoked||!distinct_mapping||
       s->generation==UINT64_MAX||generation!=s->generation+1){fail(s);return 0;}
    s->generation=generation;s->fresh=1;s->phase=BOOTING;return 1;
}
static int boot_complete(session *s,int reset_ok){
    if(s->phase!=BOOTING||!reset_ok){fail(s);return 0;}
    s->reset_ok=1;s->phase=WAIT_READY;return 1;
}
static int ready(session *s,uint64_t gen){
    if(s->phase!=WAIT_READY||!s->fresh||!s->reset_ok||gen!=s->generation){fail(s);return 0;}
    s->ready=1;s->phase=ONLINE;return 1;
}
static int activate(session *s){
    if(s->phase!=ONLINE||!s->ready)return 0;
    s->phase=RUNNING;return 1;
}
static session start(void){return(session){.phase=RUNNING,.generation=7};}
static void normal(void){
    session s=start();assert(submit(&s));assert(request_stop(&s));
    assert(!submit(&s)&&!drain(&s));assert(acknowledge(&s)&&drain(&s));
    assert(retire(&s,1,1)&&boot(&s,1,8)&&boot_complete(&s,1)&&ready(&s,8)&&activate(&s));
    assert(submit(&s));tmr_peer p;uint8_t b[24];
    assert(tmr_init(&p,1,8,0)&&tmr_encode(b,24,1,8,1)&&tmr_receive(&p,b,24,1));
    assert(acknowledge(&s));
    puts("PASS: drained stop, retirement, fresh generation and resumed TMR1");
}
int main(void){
    normal();
    session s=start();assert(submit(&s)&&request_stop(&s));
    fail(&s);assert(s.in_flight==1&&!submit(&s)&&!acknowledge(&s)&&!activate(&s));
    puts("PASS: drain timeout quarantines outstanding transfer without implicit ACK");
    for(unsigned i=0;i<4;i++){
        s=start();assert(request_stop(&s)&&drain(&s));
        if(i==0){assert(!retire(&s,0,1));}
        else if(i==1){assert(!retire(&s,1,0));}
        else if(i==2){assert(retire(&s,1,1)&&!boot(&s,0,8));}
        else {assert(retire(&s,1,1)&&!boot(&s,1,7));}
        assert(s.phase==QUARANTINED&&!submit(&s));
    }
    puts("PASS: no recovery without quiescence, revocation, distinct mapping and generation");
    s=start();assert(request_stop(&s)&&drain(&s)&&retire(&s,1,1)&&boot(&s,1,8));
    assert(!boot_complete(&s,0)&&s.phase==QUARANTINED&&!submit(&s));
    s=start();assert(request_stop(&s)&&drain(&s)&&retire(&s,1,1)&&boot(&s,1,8)&&boot_complete(&s,1));
    assert(!ready(&s,7)&&s.phase==QUARANTINED&&!submit(&s));
    s=start();s.generation=UINT64_MAX;
    assert(request_stop(&s)&&drain(&s)&&retire(&s,1,1)&&!boot(&s,1,0));
    puts("PASS: failed reset, stale readiness and generation exhaustion denied");
    return 0;
}
