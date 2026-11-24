#include "page_probe_stats.h"

#include <string.h>

bool exodus_probe_page(const uint8_t *page, struct exodus_probe_stats *stats)
{
    uint8_t encoded[EXODUS_PAGE_BYTES], decoded[EXODUS_PAGE_BYTES];
    size_t size = exodus_page_encode(page, encoded);
    if (!exodus_page_decode(encoded, size, decoded) || memcmp(decoded, page, sizeof(decoded)) != 0)
        return false;
    ++stats->pages;
    stats->sparse_pages += size < EXODUS_PAGE_BYTES;
    stats->payload_bytes += size;
    return true;
}

enum exodus_probe_result exodus_probe_batch(FILE *input, size_t max_pages,
                                             struct exodus_probe_stats *stats)
{
    uint8_t page[EXODUS_PAGE_BYTES];
    for (size_t i = 0; i < max_pages; ++i) {
        size_t count = fread(page, 1, sizeof(page), input);
        if (count < sizeof(page)) {
            if (ferror(input))
                return EXODUS_PROBE_IO_ERROR;
            stats->trailing_bytes = count;
            stats->payload_bytes += count;
            return EXODUS_PROBE_DONE;
        }
        if (!exodus_probe_page(page, stats))
            return EXODUS_PROBE_MISMATCH;
    }
    return EXODUS_PROBE_MORE;
}
