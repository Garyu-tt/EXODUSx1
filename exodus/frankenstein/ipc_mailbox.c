/* R33 reference mailbox: one writer and one reader per direction.
 * Requires coherent normal shared RAM and lock-free 64-bit atomics.
 * Mediator initializes once before both endpoints attach; never reset live.
 * No physical addresses, kernel mapping, device access or authentication.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdatomic.h>
#include <string.h>
#include "ipc_native.c"

enum { TMR_FAULT = -1, TMR_IDLE = 0, TMR_OK = 1 };
typedef struct {
    _Alignas(64) _Atomic uint64_t published;
    _Atomic uint64_t consumed;
    _Atomic uint64_t words[3];
    _Atomic uint64_t claimed; /* permanent per-generation producer claim */
} tmr_slot;
typedef struct { tmr_slot direction[2]; } tmr_mailbox;
typedef struct {
    tmr_slot *tx, *rx;
    tmr_peer peer;
    uint64_t generation, sent;
    uint8_t role, closed, peer_claim_seen;
} tmr_endpoint;

/* The shared ABI must be identical for ARMv7 and AArch64. Local
 * endpoint objects (including pointers) are never shared. Both domains
 * must use little endian, coherent normal RAM and verified atomics. */
_Static_assert(sizeof(_Atomic uint64_t) == 8, "atomic width");
_Static_assert(_Alignof(tmr_slot) == 64, "slot alignment");
_Static_assert(offsetof(tmr_slot, published) == 0, "published offset");
_Static_assert(offsetof(tmr_slot, consumed) == 8, "consumed offset");
_Static_assert(offsetof(tmr_slot, words) == 16, "words offset");
_Static_assert(offsetof(tmr_slot, claimed) == 40, "claimed offset");
_Static_assert(sizeof(tmr_slot) == 64, "mailbox layout");
_Static_assert(offsetof(tmr_mailbox, direction[1]) == 64, "slot stride");
_Static_assert(sizeof(tmr_mailbox) == 128, "mailbox layout");

static int mailbox_init(tmr_mailbox *m) {
    for (unsigned i = 0; i < 2; ++i) {
        tmr_slot *s = &m->direction[i];
        atomic_init(&s->published, 0);
        atomic_init(&s->consumed, 0);
        atomic_init(&s->claimed, 0);
        if (!atomic_is_lock_free(&s->claimed)) return 0;
        if (!atomic_is_lock_free(&s->published) || !atomic_is_lock_free(&s->consumed)) return 0;
        for (unsigned j = 0; j < 3; ++j) {
            atomic_init(&s->words[j], 0);
            if (!atomic_is_lock_free(&s->words[j])) return 0;
        }
    }
    return 1;
}
static int fault(tmr_endpoint *e) {
    e->closed = 1; e->peer.closed = 1;
    return TMR_FAULT;
}
static int attach(tmr_endpoint *e, tmr_mailbox *m, uint8_t role,
                  uint64_t generation, uint64_t peer_generation, uint64_t now) {
    *e = (tmr_endpoint){0};
    e->closed = 1;
    if (!valid_role(role) || !generation ||
        !tmr_init(&e->peer, 3-role, peer_generation, now)) return TMR_FAULT;
    e->tx = &m->direction[role-1]; e->rx = &m->direction[2-role];
    /* Each role is claimed exactly once, including before its first frame.
     * The remote producer may already have published; do not clear its slot.
     * A failed/crashed claimant is never replaced in this live mailbox. */
    if (atomic_load_explicit(&e->tx->published, memory_order_acquire) ||
        atomic_load_explicit(&e->tx->consumed, memory_order_acquire) ||
        atomic_load_explicit(&e->rx->consumed, memory_order_acquire)) return TMR_FAULT;
    uint64_t peer_claim = atomic_load_explicit(&e->rx->claimed, memory_order_acquire);
    if (peer_claim && peer_claim != peer_generation) return TMR_FAULT;
    uint64_t empty = 0;
    /* Sequential consistency across both direction claims prevents the
     * store-buffering outcome: concurrent opposing attaches cannot both
     * miss the other's incompatible generation after claiming their roles. */
    if (!atomic_compare_exchange_strong_explicit(&e->tx->claimed, &empty,
            generation, memory_order_seq_cst, memory_order_seq_cst)) return TMR_FAULT;
    uint64_t confirmed = atomic_load_explicit(&e->rx->claimed, memory_order_seq_cst);
    if ((peer_claim && confirmed != peer_claim) ||
        (confirmed && confirmed != peer_generation)) return TMR_FAULT;
    /* A post-claim rejection leaves the permanent claim occupied. Recovery
     * requires mediator quiescence and a fresh mailbox, never live rollback. */
    peer_claim = confirmed;
    e->role = role; e->generation = generation; e->closed = 0;
    e->peer_claim_seen = peer_claim != 0;
    return TMR_OK;
}
static int send_frame(tmr_endpoint *e, uint64_t now) {
    if (e->closed || !tmr_tick(&e->peer, now)) return fault(e);
    uint64_t peer_claim = atomic_load_explicit(&e->rx->claimed, memory_order_acquire);
    if (peer_claim != e->peer.generation && (peer_claim || e->peer_claim_seen))
        return fault(e);
    if (peer_claim) e->peer_claim_seen = 1;
    uint64_t pub = atomic_load_explicit(&e->tx->published, memory_order_acquire);
    uint64_t ack = atomic_load_explicit(&e->tx->consumed, memory_order_acquire);
    if (atomic_load_explicit(&e->tx->claimed, memory_order_acquire) != e->generation ||
        pub != e->sent || ack > pub || pub-ack > 1 || pub == UINT64_MAX) return fault(e);
    if (pub != ack) return TMR_IDLE; /* one frame in flight, never overwrite */
    uint8_t frame[TMR_FRAME_SIZE]; uint64_t words[3];
    if (!tmr_encode(frame, sizeof(frame), e->role, e->generation, pub+1)) return fault(e);
    memcpy(words, frame, sizeof(frame));
    for (unsigned i = 0; i < 3; ++i)
        atomic_store_explicit(&e->tx->words[i], words[i], memory_order_relaxed);
    atomic_store_explicit(&e->tx->published, ++e->sent, memory_order_release);
    return TMR_OK;
}
static int recv_frame(tmr_endpoint *e, uint64_t now) {
    if (e->closed || !tmr_tick(&e->peer, now)) return fault(e);
    uint64_t ack = atomic_load_explicit(&e->rx->consumed, memory_order_acquire);
    uint64_t pub = atomic_load_explicit(&e->rx->published, memory_order_acquire);
    if (ack != e->peer.sequence || pub < ack || pub-ack > 1) return fault(e);
    /* Read the remote claim after acquiring publication. Before the peer
     * attaches, zero is legitimate only while no frame has been published. */
    uint64_t peer_claim = atomic_load_explicit(&e->rx->claimed, memory_order_acquire);
    if (atomic_load_explicit(&e->tx->claimed, memory_order_acquire) != e->generation ||
        (peer_claim != e->peer.generation && (peer_claim || pub || e->peer_claim_seen))) return fault(e);
    if (peer_claim) e->peer_claim_seen = 1;
    if (pub == ack) return TMR_IDLE;
    uint64_t words[3]; uint8_t frame[TMR_FRAME_SIZE];
    for (unsigned i = 0; i < 3; ++i)
        words[i] = atomic_load_explicit(&e->rx->words[i], memory_order_relaxed);
    if (atomic_load_explicit(&e->rx->published, memory_order_acquire) != pub) return fault(e);
    memcpy(frame, words, sizeof(frame));
    if (!tmr_receive(&e->peer, frame, sizeof(frame), now) || e->peer.sequence != pub) return fault(e);
    atomic_store_explicit(&e->rx->consumed, pub, memory_order_release);
    return TMR_OK;
}

#ifdef TMR_MAILBOX_SELFTEST
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

static void fresh(tmr_mailbox *m, tmr_endpoint *a, tmr_endpoint *b) {
    assert(mailbox_init(m));
    assert(attach(a,m,1,100,200,0)==TMR_OK);
    assert(attach(b,m,2,200,100,0)==TMR_OK);
}
static uint64_t now_ms(void) {
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC,&ts)==0);
    return (uint64_t)ts.tv_sec*1000+(uint64_t)ts.tv_nsec/1000000;
}
static int exchange(tmr_mailbox *m, uint8_t role) {
    tmr_endpoint e;
    uint64_t start=now_ms();
    if (attach(&e,m,role,role,3-role,start)!=TMR_OK) return 1;
    while (now_ms()-start<5000) {
        uint64_t now=now_ms();
        if (e.sent<128 && send_frame(&e,now)==TMR_FAULT) return 2;
        if (recv_frame(&e,now)==TMR_FAULT) return 3;
        if (e.sent==128 && e.peer.sequence==128 &&
            atomic_load_explicit(&e.tx->consumed,memory_order_acquire)==128) return 0;
        struct timespec delay={0,1000000};
        nanosleep(&delay,NULL);
    }
    return 4;
}
int main(void) {
    tmr_mailbox m; tmr_endpoint a,b;
    fresh(&m,&a,&b);
    tmr_endpoint duplicate;
    assert(attach(&duplicate,&m,1,100,200,0)==TMR_FAULT);
    assert(attach(&duplicate,&m,2,201,100,0)==TMR_FAULT);
    assert(atomic_load(&m.direction[0].claimed)==100);
    assert(atomic_load(&m.direction[1].claimed)==200);
    puts("PASS: duplicate producer rejected before first frame");
    /* Legitimate late peer attachment remains supported, even when the
     * first producer has already published. A wrong generation must not
     * consume the other direction's permanent claim. */
    assert(mailbox_init(&m));
    assert(attach(&a,&m,1,100,200,0)==TMR_OK);
    assert(recv_frame(&a,1)==TMR_IDLE);
    assert(send_frame(&a,1)==TMR_OK);
    assert(attach(&b,&m,2,200,101,1)==TMR_FAULT);
    assert(atomic_load(&m.direction[1].claimed)==0);
    assert(attach(&b,&m,2,200,100,1)==TMR_OK);
    assert(recv_frame(&b,2)==TMR_OK);
    fresh(&m,&a,&b);
    atomic_store(&m.direction[1].claimed,201);
    assert(recv_frame(&a,1)==TMR_FAULT);
    assert(atomic_load(&m.direction[1].consumed)==0);
    fresh(&m,&a,&b);
    atomic_store(&m.direction[1].claimed,201);
    assert(send_frame(&a,1)==TMR_FAULT);
    assert(atomic_load(&m.direction[0].published)==0);
    fresh(&m,&a,&b);
    atomic_store(&m.direction[0].claimed,0);
    assert(recv_frame(&a,1)==TMR_FAULT);
    fresh(&m,&a,&b);
    assert(send_frame(&b,1)==TMR_OK && recv_frame(&a,1)==TMR_OK);
    atomic_store(&m.direction[1].claimed,0);
    assert(recv_frame(&a,2)==TMR_FAULT);
    /* A permanent remote claim may be absent only until first observation.
     * Reject a cleared claim even before the peer publishes any frame. */
    fresh(&m,&a,&b);
    atomic_store(&m.direction[0].claimed,0);
    assert(send_frame(&b,1)==TMR_FAULT);
    assert(atomic_load(&m.direction[1].published)==0);
    fresh(&m,&a,&b);
    assert(recv_frame(&a,1)==TMR_IDLE);
    atomic_store(&m.direction[1].claimed,0);
    assert(recv_frame(&a,2)==TMR_FAULT);
    fresh(&m,&a,&b);
    assert(send_frame(&a,1)==TMR_OK);
    atomic_store(&m.direction[1].claimed,0);
    assert(send_frame(&a,2)==TMR_FAULT);
    assert(atomic_load(&m.direction[0].published)==1);
    puts("PASS: mismatched or cleared claims rejected during attach, send and idle receive");
    fresh(&m,&a,&b);
    assert(recv_frame(&a,1)==TMR_IDLE);
    assert(send_frame(&a,1)==TMR_OK && send_frame(&a,1)==TMR_IDLE);
    assert(send_frame(&b,1)==TMR_OK);
    assert(recv_frame(&b,2)==TMR_OK && recv_frame(&a,2)==TMR_OK);
    assert(send_frame(&a,3)==TMR_OK && recv_frame(&b,3)==TMR_OK);
    assert(b.peer.sequence==2);
    puts("PASS: bidirectional delivery and backpressure");

    /* Deliberately seed the completed prefix in a quiescent test fixture.
     * Both generations have nonzero high words; each direction crosses the
     * 32-bit sequence boundary without changing the existing wire format. */
    const uint64_t ga=UINT64_C(0x1234567800000064);
    const uint64_t gb=UINT64_C(0x87654321000000c8);
    const uint64_t before=UINT64_C(0xffffffff);
    assert(mailbox_init(&m));
    assert(attach(&a,&m,1,ga,gb,0)==TMR_OK);
    assert(attach(&b,&m,2,gb,ga ^ (UINT64_C(1)<<32),0)==TMR_FAULT);
    assert(atomic_load(&m.direction[1].claimed)==0);
    assert(attach(&b,&m,2,gb,ga,0)==TMR_OK);
    for (unsigned i=0;i<2;++i) {
        atomic_store(&m.direction[i].published,before);
        atomic_store(&m.direction[i].consumed,before);
    }
    a.sent=b.sent=a.peer.sequence=b.peer.sequence=before;
    assert(send_frame(&a,1)==TMR_OK && send_frame(&b,1)==TMR_OK);
    assert(recv_frame(&a,2)==TMR_OK && recv_frame(&b,2)==TMR_OK);
    assert(a.peer.sequence==before+1 && b.peer.sequence==before+1);
    assert(a.sent==before+1 && b.sent==before+1);
    assert(atomic_load(&m.direction[0].consumed)==before+1);
    assert(atomic_load(&m.direction[1].consumed)==before+1);
    assert(send_frame(&a,3)==TMR_OK && recv_frame(&b,4)==TMR_OK);
    assert(b.peer.sequence==before+2);
    puts("PASS: full-width mailbox generations and counters across 32-bit boundary");
    fresh(&m,&a,&b);
    assert(send_frame(&a,1)==TMR_OK);
    atomic_fetch_xor(&m.direction[0].words[0],UINT64_C(1));
    assert(recv_frame(&b,2)==TMR_FAULT && recv_frame(&b,3)==TMR_FAULT);
    assert(atomic_load(&m.direction[0].consumed)==0);
    fresh(&m,&a,&b);
    atomic_store(&m.direction[0].published,2);
    assert(recv_frame(&b,1)==TMR_FAULT);
    fresh(&m,&a,&b);
    atomic_store(&m.direction[0].consumed,1);
    assert(send_frame(&a,1)==TMR_FAULT);
    fresh(&m,&a,&b);
    assert(send_frame(&a,1)==TMR_OK && recv_frame(&b,1)==TMR_OK);
    atomic_store(&m.direction[0].published,0);
    assert(recv_frame(&b,2)==TMR_FAULT);
    assert(attach(&a,&m,1,101,200,3)==TMR_FAULT);
    fresh(&m,&a,&b);
    assert(send_frame(&a,1)==TMR_OK);
    atomic_store(&m.direction[0].claimed,101);
    assert(recv_frame(&b,2)==TMR_FAULT);
    assert(atomic_load(&m.direction[0].consumed)==0);
    assert(send_frame(&a,2)==TMR_FAULT);
    puts("PASS: malformed slot, invalid counters, replay and unsafe reattach rejected");
    fresh(&m,&a,&b);
    assert(recv_frame(&a,3000)==TMR_FAULT);
    assert(send_frame(&a,3001)==TMR_FAULT);
    puts("PASS: expired peer closes channel");
    /* POSIX shared mapping of an unlinked temporary file, not a device. */
    FILE *f=tmpfile(); assert(f);
    int fd=fileno(f); assert(ftruncate(fd,sizeof(m))==0);
    tmr_mailbox *shared=mmap(NULL,sizeof(m),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    assert(shared!=MAP_FAILED && mailbox_init(shared));
    /* Race two processes for the same role on a fresh mapping. Exactly
     * one CAS must win; even after the winner exits, the claim persists. */
    pid_t race=fork(); assert(race>=0);
    if (race==0) {
        tmr_endpoint contender;
        _exit(attach(&contender,shared,1,1,2,now_ms())==TMR_OK ? 0 : 1);
    }
    tmr_endpoint contender;
    int won=attach(&contender,shared,1,1,2,now_ms())==TMR_OK;
    int race_status;
    assert(waitpid(race,&race_status,0)==race && WIFEXITED(race_status));
    assert(WEXITSTATUS(race_status)==(won ? 1 : 0));
    assert(attach(&contender,shared,1,1,2,now_ms())==TMR_FAULT);
    puts("PASS: concurrent duplicate role claim has exactly one winner");
    /* Opposing producers may race with incompatible peer expectations.
     * Each expects generation 99, while the actual claims are 1 and 2.
     * Never admit both; a post-CAS failure must remain closed. */
    for (unsigned trial=0; trial<64; ++trial) {
        assert(mailbox_init(shared));
        int gate[2]; assert(pipe(gate)==0);
        pid_t opposing=fork(); assert(opposing>=0);
        if (opposing==0) {
            close(gate[1]);
            char go;
            if (read(gate[0],&go,1)!=1) _exit(3);
            close(gate[0]);
            tmr_endpoint other;
            int rc=attach(&other,shared,2,2,99,now_ms());
            if (rc==TMR_FAULT && (!other.closed || send_frame(&other,now_ms())!=TMR_FAULT)) _exit(4);
            _exit(rc==TMR_OK ? 0 : rc==TMR_FAULT ? 1 : 5);
        }
        close(gate[0]);
        assert(write(gate[1],"x",1)==1);
        close(gate[1]);
        tmr_endpoint local;
        int rc=attach(&local,shared,1,1,99,now_ms());
        assert(rc==TMR_OK || rc==TMR_FAULT);
        if (rc==TMR_FAULT) assert(local.closed && send_frame(&local,now_ms())==TMR_FAULT);
        int status;
        assert(waitpid(opposing,&status,0)==opposing && WIFEXITED(status));
        assert(WEXITSTATUS(status)==0 || WEXITSTATUS(status)==1);
        assert(rc!=TMR_OK || WEXITSTATUS(status)!=0);
        if (rc==TMR_OK && atomic_load(&shared->direction[1].claimed))
            assert(send_frame(&local,now_ms())==TMR_FAULT);
        assert(atomic_load(&shared->direction[0].published)==0);
        assert(atomic_load(&shared->direction[1].published)==0);
    }
    puts("PASS: opposing incompatible generation claims cannot both attach");
    /* Both contenders are quiescent; reinitialize only the test fixture. */
    assert(mailbox_init(shared));
    pid_t pid=fork(); assert(pid>=0);
    if (pid==0) _exit(exchange(shared,2));
    int result=exchange(shared,1), status;
    assert(waitpid(pid,&status,0)==pid);
    assert(result==0 && WIFEXITED(status) && WEXITSTATUS(status)==0);
    assert(munmap(shared,sizeof(m))==0 && fclose(f)==0);
    puts("PASS: two processes exchange 128 heartbeats each in shared memory");
    return 0;
}
#endif
