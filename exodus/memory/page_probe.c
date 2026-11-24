#include "page_probe_stats.h"

#include <inttypes.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s SAMPLE\n", argv[0]);
        return EXIT_FAILURE;
    }
    FILE *input = fopen(argv[1], "rb");
    if (!input) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    struct exodus_probe_stats stats = {0};
    enum exodus_probe_result result;
    do {
        result = exodus_probe_batch(input, 128, &stats);
    } while (result == EXODUS_PROBE_MORE);
    if (result == EXODUS_PROBE_IO_ERROR)
        perror(argv[1]);
    if (result == EXODUS_PROBE_MISMATCH)
        fprintf(stderr, "Round-trip verification failed at page %" PRIu64 "\n", stats.pages);
    if (fclose(input) != 0) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    if (result != EXODUS_PROBE_DONE)
        return EXIT_FAILURE;

    printf("{\"pages\":%" PRIu64 ",\"sparse_pages\":%" PRIu64
           ",\"word_run_pages\":%" PRIu64
           ",\"input_bytes\":%" PRIu64 ",\"payload_bytes\":%" PRIu64
           ",\"length_index_bytes\":%" PRIu64 ",\"trailing_bytes\":%zu}\n",
           stats.pages, stats.sparse_pages, stats.word_run_pages,
           stats.pages * EXODUS_PAGE_BYTES + stats.trailing_bytes,
           stats.payload_bytes, stats.pages * 2, stats.trailing_bytes);
    return EXIT_SUCCESS;
}
