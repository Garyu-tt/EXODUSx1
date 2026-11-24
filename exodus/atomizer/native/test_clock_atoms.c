#include "clock_atoms.h"
#include <assert.h>
#include <stdio.h>

static uint64_t ticks(void *context) { return *(uint64_t *)context; }
int main(void) {
    uint64_t now = 19200123, ms = 99;
    uint32_t low = 99;
    int64_t signed_value = -1;
    ex_clock c = {ticks, &now, 19200000, 123, true};
    assert(ex_qpf(&c, &signed_value) && signed_value == 19200000);
    assert(ex_qpc(&c, &signed_value) && signed_value == 19200123);
    assert(ex_uptime64(&c, &ms) && ms == 1000);
    now = 124;
    assert(ex_uptime64(&c, &ms) && ms == 0);
    c.frequency = 1000;
    now = c.boot_tick + UINT64_C(4294967297);
    assert(ex_uptime32(&c, &low) && low == 1);
    assert(ex_uptime64(&c, &ms) && ms == UINT64_C(4294967297));
    c.boot_epoch_known = false;
    ms = 99;
    assert(!ex_uptime64(&c, &ms) && ms == 99);
    assert(ex_qpc(&c, &signed_value));
    c.boot_epoch_known = true;
    now = 122;
    assert(!ex_uptime64(&c, &ms) && ms == 99);
    now = UINT64_MAX;
    c.frequency = 1;
    assert(!ex_uptime64(&c, &ms) && ms == 99);
    signed_value = -1;
    assert(!ex_qpc(&c, &signed_value) && signed_value == -1);
    c.frequency = 1000;
    assert(ex_uptime64(&c, &ms) && ms == UINT64_MAX - 123);
    c.frequency = 0;
    assert(!ex_qpf(&c, &signed_value) && signed_value == -1);
    assert(!ex_qpc(NULL, &signed_value));
    assert(!ex_uptime32(&c, NULL));
    c.frequency = UINT64_MAX;
    assert(!ex_qpf(&c, &signed_value));
    puts("16 assertions passed");
    return 0;
}
