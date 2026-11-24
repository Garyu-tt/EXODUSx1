#include "page_probe_stats.h"

#include <inttypes.h>
#include <stdlib.h>
#if defined(_MSC_VER)
#include <share.h>
#endif

static int measure_sample(FILE *input, const char *error_label)
{
    if (!input) {
        perror(error_label);
        return EXIT_FAILURE;
    }

    struct exodus_probe_stats stats = {0};
    enum exodus_probe_result result;
    do {
        result = exodus_probe_batch(input, 128, &stats);
    } while (result == EXODUS_PROBE_MORE);
    if (result == EXODUS_PROBE_IO_ERROR)
        perror(error_label);
    if (result == EXODUS_PROBE_MISMATCH)
        fprintf(stderr, "Round-trip verification failed at page %" PRIu64 "\n", stats.pages);
    if (fclose(input) != 0) {
        perror(error_label);
        return EXIT_FAILURE;
    }
    if (result != EXODUS_PROBE_DONE)
        return EXIT_FAILURE;

    printf("{\"pages\":%" PRIu64 ",\"sparse_pages\":%" PRIu64
           ",\"input_bytes\":%" PRIu64 ",\"payload_bytes\":%" PRIu64
           ",\"length_index_bytes\":%" PRIu64 ",\"trailing_bytes\":%zu}\n",
           stats.pages, stats.sparse_pages, stats.pages * EXODUS_PAGE_BYTES + stats.trailing_bytes,
           stats.payload_bytes, stats.pages * 2, stats.trailing_bytes);
    return EXIT_SUCCESS;
}

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
int wmain(int argc, wchar_t **argv)
{
    if (argc != 2) {
        fputs("Usage: page_probe SAMPLE\n", stderr);
        return EXIT_FAILURE;
    }
#if defined(_MSC_VER)
    FILE *input = _wfsopen(argv[1], L"rb", _SH_DENYNO);
#else
    FILE *input = _wfopen(argv[1], L"rb");
#endif
    return measure_sample(input, "page_probe sample");
}
#else
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s SAMPLE\n", argv[0]);
        return EXIT_FAILURE;
    }
    return measure_sample(fopen(argv[1], "rb"), argv[1]);
}
#endif
