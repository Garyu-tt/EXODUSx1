#ifndef EXODUS_KERNEL_BRIDGE_H
#define EXODUS_KERNEL_BRIDGE_H
#include <stdint.h>
#include <stddef.h>
enum exk_field { EXK_PID, EXK_TID, EXK_MEMORY_LIMIT, EXK_MEMORY_USED, EXK_COUNT };
enum exk_status { EXK_OK, EXK_UNSUPPORTED, EXK_INVALID, EXK_NATIVE_ERROR };
#define EXK_ABI_V1 UINT32_C(1)
#define EXK_ALL_FIELDS ((UINT32_C(1) << EXK_COUNT) - UINT32_C(1))
typedef uint32_t (*exk_query_fn)(void *, unsigned, uint64_t *);
struct exk_provider {
    uint32_t abi_version, capabilities;
    exk_query_fn query;
    void *context;
};
struct exk_value {
    enum exk_status status;
    uint32_t native_result;
    uint64_t value;
    unsigned native_called;
};
static inline struct exk_value exk_query(const struct exk_provider *p, unsigned field) {
    struct exk_value r = {EXK_INVALID, 0, 0, 0};
    if (!p || p->abi_version != EXK_ABI_V1 || field >= EXK_COUNT ||
        (p->capabilities & ~EXK_ALL_FIELDS) != 0 ||
        (!p->query && p->capabilities != 0)) return r;
    if (!(p->capabilities & (UINT32_C(1) << field))) {
        r.status = EXK_UNSUPPORTED;
        return r;
    }
    uint64_t value = 0;
    r.native_called = 1;
    r.native_result = p->query(p->context, field, &value);
    r.status = r.native_result == 0 ? EXK_OK : EXK_NATIVE_ERROR;
    if (r.status == EXK_OK) r.value = value;
    return r;
}
#endif
