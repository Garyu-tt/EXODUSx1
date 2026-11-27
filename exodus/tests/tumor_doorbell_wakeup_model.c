/* Abstract lost-wakeup model. Notification is advisory, not an ACK.
 * Enumerate one producer publication, optional interrupt delivery, and
 * receiver check/arm/recheck/sleep/timer. No physical IRQ programming.
 */
#include <assert.h>
#include <stdio.h>
enum { PUBLISH, IRQ, CHECK, ARM, RECHECK, SLEEP, TIMER, EVENTS };
typedef struct {
    unsigned pub,seen,armed,pending,sleeping,consumed;
    unsigned missed,checks,timer_wake,irq_wake;
} state;
static void poll(state *s){
    ++s->checks;
    if(s->pub!=s->seen){
        s->seen=s->pub;++s->consumed;s->sleeping=0;s->pending=0;
    }
}
static void apply(state *s,unsigned e,int guarded,int drop){
    switch(e){
    case PUBLISH:s->pub=1;break;
    case IRQ:
        if(drop)break;
        if(s->armed){
            s->pending=1;
            if(s->sleeping){s->sleeping=0;++s->irq_wake;poll(s);}
        }
        break;
    case CHECK:poll(s);break;
    case ARM:s->armed=1;break;
    case RECHECK:if(guarded)poll(s);break;
    case SLEEP:
        if(s->consumed)break;
        if(guarded&&s->pending){s->pending=0;poll(s);break;}
        s->sleeping=1;
        if(s->pub!=s->seen)++s->missed;
        break;
    case TIMER:
        if(s->sleeping){s->sleeping=0;++s->timer_wake;}
        poll(s);break;
    }
}
static unsigned schedules,unsafe,guarded_with_timer,guarded_missed,drop_recovered,delivered_recovered;
static void visit(unsigned *order,unsigned depth,unsigned used){
    if(depth==EVENTS){
        unsigned pos[EVENTS];
        for(unsigned i=0;i<EVENTS;i++)pos[order[i]]=i;
        if(!(pos[PUBLISH]<pos[IRQ]&&pos[CHECK]<pos[ARM]&&
             pos[ARM]<pos[RECHECK]&&pos[RECHECK]<pos[SLEEP]&&
             pos[SLEEP]<pos[TIMER]&&pos[PUBLISH]<pos[TIMER]))return;
        ++schedules;
        for(int drop=0;drop<=1;drop++){
            state n={0},g={0};
            for(unsigned i=0;i<EVENTS;i++){
                apply(&n,order[i],0,drop);
                apply(&g,order[i],1,drop);
            }
            unsafe+=n.missed;
            guarded_missed+=g.missed;
            assert(g.consumed==1&&g.seen==1&&!g.sleeping);
            guarded_with_timer+=g.timer_wake;
            if(drop)++drop_recovered;else ++delivered_recovered;
        }
        return;
    }
    for(unsigned i=0;i<EVENTS;i++)if(!(used&(1u<<i))){
        order[depth]=i;visit(order,depth+1,used|(1u<<i));
    }
}
int main(void){
    unsigned order[EVENTS];visit(order,0,0);
    assert(schedules>0&&unsafe>0&&guarded_missed>0);
    assert(drop_recovered==schedules&&delivered_recovered==schedules);
    printf("PASS: lost-wakeup model %u schedules, %u delivered and %u dropped IRQ cases recovered\n",schedules,delivered_recovered,drop_recovered);
    printf("PASS: missed wake observations naive=%u guarded=%u; timer wakeups=%u\n",unsafe,guarded_missed,guarded_with_timer);
    /* Specific negative control: publication and IRQ between check/arm.
     * Without recheck or timer, the receiver remains asleep.
     */
    state n={0},g={0};
    const unsigned race[]={CHECK,PUBLISH,IRQ,ARM,RECHECK,SLEEP};
    for(unsigned i=0;i<sizeof(race)/sizeof(race[0]);i++){
        apply(&n,race[i],0,0);apply(&g,race[i],1,0);
    }
    assert(n.sleeping&&n.consumed==0&&g.consumed==1&&!g.sleeping);
    puts("PASS: arm-and-recheck closes pre-arm interrupt race");
    return 0;
}
