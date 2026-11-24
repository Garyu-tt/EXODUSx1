#ifndef EXODUS_CLOCK_ATOMS_H
#define EXODUS_CLOCK_ATOMS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Host-side outputs only; never pass an unchecked guest pointer. */
typedef struct {
    uint64_t (*read_ticks)(void *context);
    void *context;
    uint64_t frequency;
    uint64_t boot_tick;
    bool boot_epoch_known;
} ex_clock;

static inline bool ex_clock_valid(const ex_clock *c) {
    return c && c->read_ticks && c->frequency && c->frequency <= INT64_MAX;
}

static inline bool ex_qpf(const ex_clock *c, int64_t *out) {
    if (!out || !ex_clock_valid(c)) return false;
    *out = (int64_t)c->frequency;
    return true;
}

static inline bool ex_qpc(const ex_clock *c, int64_t *out) {
    if (!out || !ex_clock_valid(c)) return false;
    uint64_t ticks = c->read_ticks(c->context);
    if (ticks > INT64_MAX) return false;
    *out = (int64_t)ticks;
    return true;
}

static inline bool ex_uptime64(const ex_clock *c, uint64_t *out) {
    if (!out || !ex_clock_valid(c) || !c->boot_epoch_known) return false;
    uint64_t ticks = c->read_ticks(c->context);
    if (ticks < c->boot_tick) return false;
    /* Wide intermediate supported by GCC/Clang, including devkitA64. */
    __uint128_t ms = (__uint128_t)(ticks - c->boot_tick) * 1000u / c->frequency;
    if (ms > UINT64_MAX) return false;
    *out = (uint64_t)ms;
    return true;
}

static inline bool ex_uptime32(const ex_clock *c, uint32_t *out) {
    uint64_t ms;
    if (!out || !ex_uptime64(c, &ms)) return false;
    *out = (uint32_t)ms;
    return true;
}

#ifdef __SWITCH__
#include <switch.h>
static inline uint64_t ex_nx_ticks(void *context) {
    (void)context;
    return armGetSystemTick();
}
/* Caller must establish a Windows-compatible boot epoch separately. */
static inline ex_clock ex_nx_clock(void) {
    return (ex_clock){ex_nx_ticks, NULL, armGetSystemTickFreq(), 0, false};
}
#endif
#endif
