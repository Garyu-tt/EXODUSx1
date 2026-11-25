#ifndef EXODUS_PAGE_SNAPSHOT_H
#define EXODUS_PAGE_SNAPSHOT_H

#include "page_codec.h"

enum { EXODUS_SNAPSHOT_MAX_BYTES = 1 + EXODUS_PAGE_BYTES };

/* Encode a 4096-byte page into an EXODUS_SNAPSHOT_MAX_BYTES buffer disjoint
 * from both inputs. The two read-only input pages may be identical.
 * An optional immutable reference may reduce the frame size. Keep that page
 * available for decoding when the returned frame starts with byte 1. */
size_t exodus_snapshot_encode(const uint8_t *page, const uint8_t *reference,
                              uint8_t *frame);

/* Keep the frame disjoint from both pages. A reference-dependent frame
 * checks the reference's CRC-32 before writing. Output and reference must
 * be disjoint or exactly the same buffer.
 * Invalid frames or reference checksums leave the destination unchanged. */
bool exodus_snapshot_decode(const uint8_t *frame, size_t size,
                            const uint8_t *reference, uint8_t *page);

#endif
