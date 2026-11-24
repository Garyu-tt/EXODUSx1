#include "page_codec.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    uint8_t page[EXODUS_PAGE_BYTES], encoded[EXODUS_PAGE_BYTES], decoded[EXODUS_PAGE_BYTES];
    uint64_t pages = 0, sparse = 0, payload = 0;
    size_t count;
    while ((count = fread(page, 1, sizeof(page), input)) == sizeof(page)) {
        size_t size = exodus_page_encode(page, encoded);
        if (!exodus_page_decode(encoded, size, decoded) || memcmp(decoded, page, sizeof(page)) != 0) {
            fprintf(stderr, "Round-trip verification failed at page %" PRIu64 "\n", pages);
            fclose(input);
            return EXIT_FAILURE;
        }
        ++pages;
        sparse += size < sizeof(page);
        payload += size;
    }
    if (ferror(input)) {
        perror(argv[1]);
        fclose(input);
        return EXIT_FAILURE;
    }
    if (fclose(input) != 0) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    /* A trailing fragment stays raw. Two bytes per full page can hold the
     * encoded length; allocator overhead and random-access offsets vary by caller. */
    printf("{\"pages\":%" PRIu64 ",\"sparse_pages\":%" PRIu64
           ",\"input_bytes\":%" PRIu64 ",\"payload_bytes\":%" PRIu64
           ",\"length_index_bytes\":%" PRIu64 ",\"trailing_bytes\":%zu}\n",
           pages, sparse, pages * EXODUS_PAGE_BYTES + count, payload + count, pages * 2, count);
    return EXIT_SUCCESS;
}
