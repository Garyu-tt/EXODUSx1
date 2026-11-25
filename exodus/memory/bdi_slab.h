#ifndef EXODUS_BDI_SLAB_H
#define EXODUS_BDI_SLAB_H
#include "page_codec.h"

/* Internal EX-005 hot-page backing primitive. Caller owns separate buffers,
 * zeroes reference arrays and page descriptors, and serializes all access.
 * Do not copy active descriptors except through clone. Slots are exclusive
 * to this pool, and each live page owns one reference per block. */
enum { EXODUS_BDI_SLAB_BLOCKS=64, EXODUS_BDI_SLAB_LIMIT=32768 };
/* One bit per slot, plus two levels of free-word summaries.
 * Optional caller-owned index. Capacities remain fixed while attached. */
struct exodus_bdi_free_class {
    uint64_t bits[512], summary[8];
    uint8_t top;
    size_t capacity;
};
struct exodus_bdi_free_index { struct exodus_bdi_free_class cls[2]; };
struct exodus_bdi_slab {
    uint8_t *small, *raw;
    uint16_t *small_refs, *raw_refs;
    size_t small_capacity, raw_capacity;
    struct exodus_bdi_free_index *free_index;
};
struct exodus_bdi_slab_page {
    uint16_t ids[EXODUS_BDI_SLAB_BLOCKS];
    bool active;
};
/* Import, clone and write are failure-atomic. Release clears the descriptor.
 * Inputs/outputs must not overlap pool storage or other active descriptors.
 * No global dedup lookup: cloning shares blocks of a known identical page. */
bool exodus_bdi_slab_import(struct exodus_bdi_slab *,struct exodus_bdi_slab_page *,const uint8_t *);
bool exodus_bdi_slab_clone(struct exodus_bdi_slab *,struct exodus_bdi_slab_page *,const struct exodus_bdi_slab_page *);
bool exodus_bdi_slab_release(struct exodus_bdi_slab *,struct exodus_bdi_slab_page *);
bool exodus_bdi_slab_restore(const struct exodus_bdi_slab *,const struct exodus_bdi_slab_page *,uint8_t *);
bool exodus_bdi_slab_read(const struct exodus_bdi_slab *,const struct exodus_bdi_slab_page *,size_t,uint8_t out[4]);
bool exodus_bdi_slab_write(struct exodus_bdi_slab *,struct exodus_bdi_slab_page *,size_t,const uint8_t value[4]);
/* Build from current reference counts; no payload changes. Attach only
 * with an independent index buffer. Detach before changing capacities. */
bool exodus_bdi_slab_attach_index(struct exodus_bdi_slab *,struct exodus_bdi_free_index *);
void exodus_bdi_slab_detach_index(struct exodus_bdi_slab *);
/* Audit the derived index against all slot reference counts. */
bool exodus_bdi_slab_check_index(const struct exodus_bdi_slab *);
#endif
