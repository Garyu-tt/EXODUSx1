/* Deterministic write-back cache model for the IPC32 ownership contract.
 * Not a Tegra cache implementation. Clean/invalidate are abstract operations
 * that complete synchronously; no timing or physical atomicity claim.
 */
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
enum { TX=0, RX=1, PUB=0, ACK=1, N=64 };
typedef struct {
    uint8_t ram[2][N], cache[2][2][N];
    unsigned valid[2][2], dirty[2][2];
} model;
static uint8_t *line(model *m,unsigned cpu,unsigned slot){
    if(!m->valid[cpu][slot]){
        memcpy(m->cache[cpu][slot],m->ram[slot],N);
        m->valid[cpu][slot]=1;
    }
    return m->cache[cpu][slot];
}
static void store32(model *m,unsigned cpu,unsigned slot,unsigned off,uint32_t v){
    assert(cpu==slot && off+4<=N);
    uint8_t *p=line(m,cpu,slot)+off;
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));
    m->dirty[cpu][slot]=1;
}
static uint32_t load32(model *m,unsigned cpu,unsigned slot,unsigned off){
    assert(off+4<=N);
    const uint8_t *p=line(m,cpu,slot)+off;
    uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);
    return v;
}
static void clean(model *m,unsigned cpu,unsigned slot){
    assert(cpu==slot && m->valid[cpu][slot]);
    if(m->dirty[cpu][slot]){
        memcpy(m->ram[slot],m->cache[cpu][slot],N);
        m->dirty[cpu][slot]=0;
    }
}
static void invalidate(model *m,unsigned cpu,unsigned slot){
    assert(!m->dirty[cpu][slot]);m->valid[cpu][slot]=0;
}
static void publish(model *m,uint32_t token,uint32_t payload){
    store32(m,TX,PUB,4,payload);
    store32(m,TX,PUB,0,token);
    clean(m,TX,PUB);
}
static void ack(model *m,uint32_t token){
    store32(m,RX,ACK,0,token);clean(m,RX,ACK);
}
static void negative_controls(void){
    model m={0};
    assert(load32(&m,RX,PUB,0)==0);
    store32(&m,TX,PUB,4,77);store32(&m,TX,PUB,0,1);
    invalidate(&m,RX,PUB);
    assert(load32(&m,RX,PUB,0)==0); /* missing clean */
    clean(&m,TX,PUB);
    assert(load32(&m,RX,PUB,0)==0); /* missing invalidate */
    invalidate(&m,RX,PUB);
    assert(load32(&m,RX,PUB,0)==1 && load32(&m,RX,PUB,4)==77);
    assert(load32(&m,TX,ACK,0)==0);
    store32(&m,RX,ACK,0,1);
    invalidate(&m,TX,ACK);
    assert(load32(&m,TX,ACK,0)==0);
    clean(&m,RX,ACK);
    assert(load32(&m,TX,ACK,0)==0);
    invalidate(&m,TX,ACK);
    assert(load32(&m,TX,ACK,0)==1);
    puts("PASS: IPC32 cache model negative controls detect omitted maintenance");
}
static void exchanges(uint32_t prefix){
    model m={0};
    publish(&m,prefix,0);ack(&m,prefix);
    uint32_t last=prefix;
    for(unsigned i=0;i<10000;i++){
        invalidate(&m,TX,ACK);
        assert(load32(&m,TX,ACK,0)==last);
        uint32_t next=last+1, value=next^UINT32_C(0xa5a55a5a);
        /* Prime stale receiver cache before publishing. */
        invalidate(&m,RX,PUB);
        assert(load32(&m,RX,PUB,0)==last);
        publish(&m,next,value);
        assert(load32(&m,RX,PUB,0)==last);
        invalidate(&m,RX,PUB);
        assert(load32(&m,RX,PUB,0)==next);
        assert(load32(&m,RX,PUB,4)==value);
        ack(&m,next);
        assert(load32(&m,TX,ACK,0)==last);
        invalidate(&m,TX,ACK);
        assert(load32(&m,TX,ACK,0)==next);
        last=next;
    }
    assert(last==prefix+UINT32_C(10000));
}
int main(void){
    negative_controls();
    exchanges(0);exchanges(UINT32_MAX-5000);
    puts("PASS: IPC32 cache model 20000 exchanges including token rollover");
    return 0;
}
