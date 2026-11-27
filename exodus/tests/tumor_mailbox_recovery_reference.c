/* Host-only test of the pinned Tumor R33 reference, not hardware IPC.
 * Build with the original exodus/frankenstein directory on the include path.
 */
#include "ipc_mailbox.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
    tmr_mailbox retired, next;
    tmr_endpoint a,b,na,nb,probe;
    const uint64_t ga=UINT64_C(0x100000001), gb=UINT64_C(0x200000001);
    const uint64_t nga=ga+1, ngb=gb+1;
    assert(mailbox_init(&retired));
    assert(attach(&a,&retired,1,ga,gb,0)==TMR_OK);
    assert(attach(&b,&retired,2,gb,ga,0)==TMR_OK);
    assert(send_frame(&a,1)==TMR_OK);
    assert(recv_frame(&b,1)==TMR_OK);
    assert(send_frame(&b,1)==TMR_OK); /* deliberately left pending */
    assert(recv_frame(&a,3001)==TMR_FAULT);
    assert(send_frame(&b,3001)==TMR_FAULT);
    assert(atomic_load(&retired.direction[1].published)==1);
    assert(atomic_load(&retired.direction[1].consumed)==0);
    assert(attach(&probe,&retired,2,ngb,ga,3002)==TMR_FAULT);
    assert(atomic_load(&retired.direction[1].claimed)==gb);
    puts("PASS: expired mailbox retains claim and unconsumed frame");

    /* Separate storage simulates mediator-provisioned fresh mapping.
     * This fixture cannot prove real revocation or physical quiescence. */
    assert(mailbox_init(&next));
    assert(attach(&na,&next,1,nga,ngb,3002)==TMR_OK);
    assert(attach(&nb,&next,2,ngb,nga,3002)==TMR_OK);
    assert(send_frame(&a,3003)==TMR_FAULT);
    assert(send_frame(&b,3003)==TMR_FAULT);
    assert(send_frame(&na,3003)==TMR_OK);
    assert(send_frame(&nb,3003)==TMR_OK);
    assert(recv_frame(&na,3004)==TMR_OK);
    assert(recv_frame(&nb,3004)==TMR_OK);
    assert(na.peer.sequence==1 && nb.peer.sequence==1);
    puts("PASS: fresh mailbox resumes bidirectional heartbeat");

    /* Inject a well-formed frame with correct next sequence but old
     * generation; only the generation field should cause rejection. */
    uint8_t wire[TMR_FRAME_SIZE]; uint64_t words[3];
    assert(tmr_encode(wire,sizeof(wire),2,gb,2));
    memcpy(words,wire,sizeof(wire));
    for (unsigned i=0;i<3;++i)
        atomic_store_explicit(&next.direction[1].words[i],words[i],memory_order_relaxed);
    atomic_store_explicit(&next.direction[1].published,2,memory_order_release);
    assert(recv_frame(&na,3005)==TMR_FAULT);
    assert(na.closed && atomic_load(&next.direction[1].consumed)==1);
    assert(recv_frame(&na,3006)==TMR_FAULT);
    assert(atomic_load(&retired.direction[1].consumed)==0);
    puts("PASS: old-generation frame rejected without acknowledgement");
    return 0;
}
