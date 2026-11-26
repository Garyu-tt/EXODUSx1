/* Portable Tumor heartbeat codec; no hardware or transport access.
 * Trusted local mediator supplies peer role, generation and monotonic time.
 * The wire format itself does not authenticate the sender. */
#include <stdint.h>
#include <stddef.h>

#define TMR_FRAME_SIZE 24u
#define TMR_TIMEOUT_MS UINT64_C(3000)

typedef struct {
    uint64_t generation, sequence, last_rx, last_clock;
    uint8_t role, closed;
} tmr_peer;

static void put64(uint8_t *p, uint64_t v) {
    for (size_t i = 8; i; --i) { p[i-1] = (uint8_t)v; v >>= 8; }
}
static uint64_t get64(const uint8_t *p) {
    uint64_t v = 0;
    for (size_t i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}
static int valid_role(uint8_t role) { return role == 1 || role == 2; }
int tmr_encode(uint8_t *out, size_t len, uint8_t role, uint64_t generation, uint64_t sequence) {
    if (!out || len != TMR_FRAME_SIZE || !valid_role(role) || !generation || !sequence) return 0;
    out[0] = 'T'; out[1] = 'M'; out[2] = 'R'; out[3] = '1'; out[4] = 1; out[5] = role; out[6] = out[7] = 0;
    put64(out + 8, generation); put64(out + 16, sequence);
    return 1;
}
static int close_peer(tmr_peer *p) { if (p) p->closed = 1; return 0; }
int tmr_init(tmr_peer *p, uint8_t role, uint64_t generation, uint64_t now) {
    if (!p) return 0;
    *p = (tmr_peer){0}; p->closed = 1;
    if (!valid_role(role) || !generation) return 0;
    p->role = role; p->generation = generation; p->last_rx = p->last_clock = now; p->closed = 0;
    return 1;
}
int tmr_tick(tmr_peer *p, uint64_t now) {
    if (!p || p->closed || now < p->last_clock || now - p->last_rx >= TMR_TIMEOUT_MS) return close_peer(p);
    p->last_clock = now;
    return 1;
}
int tmr_receive(tmr_peer *p, const uint8_t *in, size_t len, uint64_t now) {
    if (!tmr_tick(p, now) || !in || len != TMR_FRAME_SIZE) return close_peer(p);
    if (in[0] != 'T' || in[1] != 'M' || in[2] != 'R' || in[3] != '1' || in[4] != 1 || in[5] != p->role || in[6] || in[7] ||
        get64(in + 8) != p->generation || p->sequence == UINT64_MAX ||
        get64(in + 16) != p->sequence + 1) return close_peer(p);
    p->sequence++; p->last_rx = now;
    return 1;
}
int tmr_live(tmr_peer *p, uint64_t now) { return tmr_tick(p, now) && p->sequence != 0; }

#ifdef TMR_SELFTEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    tmr_peer p;
    uint8_t b[TMR_FRAME_SIZE];
    const uint8_t expected[TMR_FRAME_SIZE] = {
        0x54,0x4d,0x52,0x31,1,1,0,0, 0,0,0,0,0,0,0,9, 0,0,0,0,0,0,0,1
    };
    assert(tmr_encode(b, sizeof(b), 1, 9, 1));
    assert(!memcmp(b, expected, sizeof(b)));
    puts("PASS: wire vector");
    assert(!tmr_encode(b, sizeof(b), 0, 9, 1));
    assert(!tmr_init(&p, 1, 0, 100) && p.closed);
    assert(!tmr_receive(&p, b, sizeof(b), 100));
    puts("PASS: invalid local initialization");
    assert(tmr_init(&p, 1, 9, 100));
    assert(!tmr_live(&p, 100));
    assert(tmr_receive(&p, b, sizeof(b), 101));
    assert(tmr_live(&p, 102));
    assert(tmr_encode(b, sizeof(b), 1, 9, 2));
    assert(tmr_receive(&p, b, sizeof(b), 103));
    assert(!tmr_receive(&p, b, sizeof(b), 104));
    assert(!tmr_live(&p, 105));
    puts("PASS: liveness and replay rejection");
    for (size_t i = 0; i < TMR_FRAME_SIZE; ++i) {
        assert(tmr_init(&p, 1, 9, 100));
        memcpy(b, expected, sizeof(b)); b[i] ^= 1;
        assert(!tmr_receive(&p, b, sizeof(b), 101) && p.closed);
    }
    puts("PASS: corrupt each wire byte (24 cases)");
    for (size_t len = 0; len <= TMR_FRAME_SIZE + 1; ++len) {
        if (len == TMR_FRAME_SIZE) continue;
        assert(tmr_init(&p, 1, 9, 100));
        assert(!tmr_receive(&p, expected, len, 101) && p.closed);
    }
    puts("PASS: truncated and oversized length (25 cases)");
    assert(tmr_init(&p, 1, 9, 100));
    assert(tmr_receive(&p, expected, sizeof(b), 101));
    assert(tmr_live(&p, 3100));
    assert(!tmr_live(&p, 3101));
    puts("PASS: deadline boundary");
    assert(tmr_init(&p, 1, 9, 100));
    assert(tmr_tick(&p, 200));
    assert(!tmr_receive(&p, expected, sizeof(b), 199));
    puts("PASS: clock rollback between polls");
    assert(tmr_init(&p, 1, 10, 100));
    assert(!tmr_receive(&p, expected, sizeof(b), 101));
    assert(tmr_init(&p, 1, 10, 100));
    assert(tmr_encode(b, sizeof(b), 1, 10, 1));
    assert(tmr_receive(&p, b, sizeof(b), 101));
    puts("PASS: fresh generation");
    p.sequence = UINT64_MAX;
    assert(!tmr_receive(&p, b, sizeof(b), 102));
    assert(tmr_init(&p, 2, UINT64_MAX, UINT64_MAX-1));
    assert(tmr_encode(b, sizeof(b), 2, UINT64_MAX, 1));
    assert(tmr_receive(&p, b, sizeof(b), UINT64_MAX));
    puts("PASS: integer boundaries");
    return 0;
}
#endif
