#ifndef EXODUS_PAGE_PROBE_STATS_H
#define EXODUS_PAGE_PROBE_STATS_H

#include "page_codec.h"
#include <stdio.h>

struct exodus_probe_stats {
    uint64_t pages;
    uint64_t sparse_pages;
    uint64_t payload_bytes;
    size_t trailing_bytes;
};

enum exodus_probe_result {
    EXODUS_PROBE_MORE,
    EXODUS_PROBE_DONE,
    EXODUS_PROBE_IO_ERROR,
    EXODUS_PROBE_MISMATCH
};

/* Update a zero-initialized or ongoing measurement only after exact recovery. */
bool exodus_probe_page(const uint8_t *page, struct exodus_probe_stats *stats);

/* Consume at most max_pages (positive). Call again only after EXODUS_PROBE_MORE.
 * At EOF a final partial page is counted as raw payload. */
enum exodus_probe_result exodus_probe_batch(FILE *input, size_t max_pages,
                                             struct exodus_probe_stats *stats);

#endif
