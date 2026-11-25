#ifndef EXODUS_REMOTE_GATE_H
#define EXODUS_REMOTE_GATE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Transport-independent reference gate. Authenticator and local owner policy
 * supply session state; never deserialize this struct from remote requests. */
typedef enum {
    EXR_DISCOVER = 0, EXR_STATUS, EXR_BATTERY, EXR_STORAGE, EXR_COUNT
} exr_op;
typedef enum {
    EXR_OK = 0, EXR_DENIED, EXR_BAD_REQUEST, EXR_UNAVAILABLE, EXR_PROVIDER_ERROR
} exr_result;
#define EXR_BIT(op) (UINT32_C(1) << (op))
#define EXR_KNOWN ((UINT32_C(1) << EXR_COUNT) - 1u)
#define EXR_RESPONSE_CAP 512u

typedef struct {
    int authenticated, locally_enabled, revoked;
    uint64_t expires_at; /* monotonic ticks in same units as now */
    uint32_t grants;
} exr_session;
typedef exr_result (*exr_read_fn)(void *ctx, exr_op op, char *out, size_t cap);
typedef struct {
    uint32_t reviewed; /* local policy, not caller-controlled */
    uint32_t available; /* local provider probe */
    exr_read_fn read;
    void *ctx;
} exr_provider;

static inline const char *exr_name(exr_op op) {
    switch (op) {
    case EXR_DISCOVER: return "capabilities.list";
    case EXR_STATUS: return "system.status";
    case EXR_BATTERY: return "battery.read";
    case EXR_STORAGE: return "storage.read";
    default: return "unknown";
    }
}

static inline exr_result exr_dispatch(const exr_session *s,
    const exr_provider *p, uint64_t now, const char *request, size_t len,
    char *out, size_t cap) {
    if (!out || cap == 0 || cap > EXR_RESPONSE_CAP) return EXR_BAD_REQUEST;
    memset(out, 0, cap);
    if (!s || !s->authenticated || !s->locally_enabled || s->revoked ||
        now >= s->expires_at) return EXR_DENIED;
    if (!request || len == 0 || len > 32) return EXR_BAD_REQUEST;
    exr_op op;
    for (op = EXR_DISCOVER; op < EXR_COUNT; ++op) {
        const char *name = exr_name(op);
        if (len == strlen(name) && !memcmp(request, name, len)) break;
    }
    if (op == EXR_COUNT) return EXR_BAD_REQUEST;
    if (!(s->grants & EXR_BIT(op))) return EXR_DENIED;
    if (!p) return EXR_UNAVAILABLE;
    uint32_t allowed = s->grants & p->reviewed & p->available & EXR_KNOWN;
    if (op == EXR_DISCOVER) {
        /* Discovery is also opt-in and never advertises denied providers. */
        if (!(allowed & EXR_BIT(op))) return EXR_UNAVAILABLE;
        if (!p->read) allowed &= EXR_BIT(EXR_DISCOVER);
        size_t used = 0;
        for (exr_op i = EXR_DISCOVER; i < EXR_COUNT; ++i) {
            if (!(allowed & EXR_BIT(i))) continue;
            const char *name = exr_name(i);
            size_t n = strlen(name);
            if (n + 1 >= cap - used) {
                memset(out, 0, cap); return EXR_PROVIDER_ERROR;
            }
            memcpy(out + used, name, n); used += n;
            out[used++] = '\n';
        }
        return EXR_OK;
    }
    if (!(allowed & EXR_BIT(op)) || !p->read) return EXR_UNAVAILABLE;
    exr_result rc = p->read(p->ctx, op, out, cap);
    if (rc != EXR_OK || !memchr(out, '\0', cap)) {
        memset(out, 0, cap);
        return EXR_PROVIDER_ERROR;
    }
    return EXR_OK;
}
#endif
