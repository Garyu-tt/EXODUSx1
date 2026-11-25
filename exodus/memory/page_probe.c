#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
#define PAGE_PROBE_NATIVE_WINDOWS 1
#elif !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "page_probe_stats.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/stat.h>
#if defined(PAGE_PROBE_NATIVE_WINDOWS)
#include <io.h>
#if defined(_MSC_VER)
#include <share.h>
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int measure_sample(FILE *input, const char *error_label)
{
    if (!input) {
        fputs("A readable regular sample file is required.\n", stderr);
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

    if (printf("{\"pages\":%" PRIu64 ",\"sparse_pages\":%" PRIu64
               ",\"word_run_pages\":%" PRIu64 ",\"repeated_tile_pages\":%" PRIu64
               ",\"bdi_pages\":%" PRIu64 ",\"input_bytes\":%" PRIu64 ",\"payload_bytes\":%" PRIu64
               ",\"length_index_bytes\":%" PRIu64 ",\"trailing_bytes\":%zu}\n",
               stats.pages, stats.sparse_pages, stats.word_run_pages, stats.repeated_tile_pages, stats.bdi_pages,
               stats.pages * EXODUS_PAGE_BYTES + stats.trailing_bytes,
               stats.payload_bytes, stats.pages * 2, stats.trailing_bytes) < 0 ||
        fflush(stdout) == EOF) {
        perror("Could not write probe results");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#if defined(PAGE_PROBE_NATIVE_WINDOWS)
static FILE *open_sample(const wchar_t *path)
{
#if defined(_MSC_VER)
    FILE *input = _wfsopen(path, L"rb", _SH_DENYNO);
#else
    FILE *input = _wfopen(path, L"rb");
#endif
    if (!input)
        return NULL;

    struct _stat64 info;
    if (_fstat64(_fileno(input), &info) == 0) {
        if ((info.st_mode & _S_IFMT) == _S_IFREG)
            return input;
        errno = EINVAL;
    }

    const int error = errno;
    fclose(input);
    errno = error;
    return NULL;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2) {
        fputs("Usage: page_probe SAMPLE\n", stderr);
        return EXIT_FAILURE;
    }
    return measure_sample(open_sample(argv[1]), "page_probe sample");
}
#else
static FILE *open_sample(const char *path)
{
    /* A FIFO with no writer would block before we could inspect the descriptor. */
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return NULL;

    struct stat info;
    if (fstat(fd, &info) == 0) {
        if (S_ISREG(info.st_mode)) {
            FILE *input = fdopen(fd, "rb");
            if (input)
                return input;
        } else {
            errno = EINVAL;
        }
    }

    const int error = errno;
    close(fd);
    errno = error;
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s SAMPLE\n", argv[0]);
        return EXIT_FAILURE;
    }
    return measure_sample(open_sample(argv[1]), argv[1]);
}
#endif
