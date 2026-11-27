/* Synthetic hardware-doorbell contract, not an MMIO driver.
 * Short message field widths follow public NVIDIA hwmailbox.h.
 * IDs here are test-only, not assigned physical channels.
 * Notification is advisory; full TMR1 generation/sequence remains in RAM.
 */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "ipc_native.c"
enum { OK=1, IDLE=0, FAULT=-1 };
typedef struct { uint32_t word;unsigned busy; } doorbell;
typedef struct { uint8_t bytes[24];uint32_t pub,ack; } lane;
static int encode_short(uint32_t id,uint32_t data,uint32_t *out){
    if(id>0x3ffu||data>0xfffffu)return 0;
    *out=UINT32_C(0x80000000)|(id<<20)|data;return 1;
}
static int decode_short(uint32_t word,uint32_t *id,uint32_t *data){
    if((word&UINT32_C(0xc0000000))!=UINT32_C(0x80000000))return 0;
    *id=(word>>20)&0x3ffu;*data=word&0xfffffu;return 1;
}
static int ring(doorbell *d,uint32_t token){
    if(d->busy)return IDLE;
    assert(encode_short(17,token&0xfffffu,&d->word));
    d->busy=1;return OK;
}
static uint32_t take(doorbell *d){
    if(!d->busy)return 0;
    d->busy=0;return d->word;
}
static int poll(lane *l,tmr_peer *p,uint32_t *local,uint64_t now){
    if(!tmr_tick(p,now))return FAULT;
    if(l->ack!=*local||(uint32_t)(l->pub-*local)>1){
        p->closed=1;return FAULT;
    }
    if(l->pub==*local)return IDLE;
    if(!tmr_receive(p,l->bytes,24,now))return FAULT;
    *local=l->pub;l->ack=*local;return OK;
}
static void trial(uint64_t prefix){
    lane l={0};doorbell d={0};tmr_peer p;
    uint32_t local=(uint32_t)prefix;
    l.pub=l.ack=local;
    assert(tmr_init(&p,1,8,0));p.sequence=prefix;
    for(unsigned i=1;i<=10000;i++){
        assert(l.pub==l.ack);
        assert(tmr_encode(l.bytes,24,1,8,prefix+i));
        ++l.pub; /* completed memory publication before notification */
        if(i%5==0){ /* drop notification */
        }else if(i%5==1){ /* occupied mailbox; no overwrite */
            assert(ring(&d,l.pub)==OK);
            uint32_t saved=d.word;
            assert(ring(&d,l.pub+1)==IDLE&&d.word==saved);
        }else{
            assert(ring(&d,l.pub)==OK);
        }
        uint32_t w=take(&d),id,data;
        if(w){assert(decode_short(w,&id,&data)&&id==17);}
        assert(poll(&l,&p,&local,i)==OK);
        /* Duplicate, old, or unrelated notification cannot consume twice. */
        assert(poll(&l,&p,&local,i)==IDLE);
        if(w){assert(decode_short(w,&id,&data));assert(poll(&l,&p,&local,i)==IDLE);}
    }
    assert(p.sequence==prefix+10000&&l.pub==l.ack);
}
int main(void){
    uint32_t w,id,data;
    assert(encode_short(1023,1048575,&w)&&w==UINT32_C(0xbfffffff));
    assert(decode_short(w,&id,&data)&&id==1023&&data==1048575);
    assert(!encode_short(1024,0,&w)&&!encode_short(0,1048576,&w));
    assert(!decode_short(0,&id,&data)&&!decode_short(UINT32_C(0xc0000000),&id,&data));
    puts("PASS: hardware mailbox short-word format and bounds");
    trial(0);trial(UINT32_C(0xfffff)-5000);trial(UINT32_MAX-5000);
    puts("PASS: 30000 TMR1 frames with dropped, busy and duplicate notifications; 20/32-bit wrap");
    lane l={0};tmr_peer p;uint32_t local=0;
    assert(tmr_init(&p,1,8,0));l.pub=1;
    assert(tmr_encode(l.bytes,24,1,7,1));
    assert(poll(&l,&p,&local,1)==FAULT&&l.ack==0);
    puts("PASS: notification cannot authorize stale generation");
    return 0;
}
