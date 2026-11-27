/* Exhaustive abstract split-publication model. No physical cache access.
 * Each of six payload words can reach shared backing independently.
 * A completion barrier must precede token publication; acknowledgement
 * must follow the entire validated read. All schedules are deterministic.
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
enum { WORDS=6, EVENTS=8 };
typedef struct {
    uint32_t backing[WORDS], next[WORDS], observed[WORDS];
    uint32_t published, ack;
    unsigned writes, read, bad, premature_ack, overwrite;
} model;
static void event(model *m,unsigned e){
    if(e<WORDS){
        m->backing[e]=m->next[e];m->writes|=1u<<e;
    }else if(e==WORDS){
        m->published=1;
    }else{
        if(m->published){
            memcpy(m->observed,m->backing,sizeof(m->observed));
            m->read=1;
            if(memcmp(m->observed,m->next,sizeof(m->next)))m->bad=1;
            else m->ack=1;
        }
    }
}
static unsigned total,safe,unsafe,guarded,guarded_bad,guarded_reads;
static void enumerate(unsigned *order,unsigned depth,unsigned used){
    if(depth!=EVENTS){
        for(unsigned i=0;i<EVENTS;i++)if(!(used&(1u<<i))){
            order[depth]=i;enumerate(order,depth+1,used|(1u<<i));
        }
        return;
    }
    model m={0},g={0};
    for(unsigned i=0;i<WORDS;i++)m.next[i]=g.next[i]=UINT32_C(0xa5a50001)+i;
    unsigned all=(1u<<WORDS)-1;
    unsigned publish_pending=0;
    for(unsigned i=0;i<EVENTS;i++){
        unsigned e=order[i];
        event(&m,e);
        if(e==WORDS && g.writes!=all)publish_pending=1;
        else event(&g,e);
        if(publish_pending && g.writes==all){
            event(&g,WORDS);publish_pending=0;
        }
    }
    ++total;
    if(m.bad)++unsafe;else ++safe;
    ++guarded;
    if(g.bad)++guarded_bad;
    if(g.read)++guarded_reads;
    assert(!g.ack || (g.read && !g.bad));
    /* A reader that arrived before publication must retry, not ACK. */
    event(&g,WORDS+1);
    assert(g.read && g.ack==1 && !g.bad);
}
static void ack_order(void){
    model m={0};
    for(unsigned i=0;i<WORDS;i++)m.next[i]=i+1;
    for(unsigned i=0;i<WORDS;i++)event(&m,i);
    event(&m,WORDS);
    /* Negative control: early ACK permits producer reuse before read. */
    m.ack=1;
    if(m.ack==m.published){
        m.backing[0]=99;
        m.overwrite=1;
    }
    event(&m,WORDS+1);
    assert(m.overwrite && m.bad);
    /* Correct protocol: no reuse until the completed read publishes ACK. */
    memset(&m,0,sizeof(m));
    for(unsigned i=0;i<WORDS;i++)m.next[i]=i+1;
    for(unsigned i=0;i<WORDS;i++)event(&m,i);
    event(&m,WORDS);
    assert(m.ack!=m.published);
    event(&m,WORDS+1);
    assert(m.ack==m.published && !m.bad);
    puts("PASS: early ACK negative control and reuse guard");
}
int main(void){
    unsigned order[EVENTS];
    enumerate(order,0,0);
    assert(total==40320 && safe+unsafe==total);
    assert(unsafe>0 && guarded==total && guarded_bad==0 && guarded_reads>0);
    printf("PASS: %u schedules; unguarded torn observations=%u; guarded torn observations=%u; immediate guarded reads=%u\n",
           total,unsafe,guarded_bad,guarded_reads);
    ack_order();
    return 0;
}
