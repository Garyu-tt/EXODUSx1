#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
#define SNAPSHOT_PROBE_NATIVE_WINDOWS 1
#elif !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "page_snapshot.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(SNAPSHOT_PROBE_NATIVE_WINDOWS)
#include <io.h>
#include <wchar.h>
#if defined(_MSC_VER)
#include <share.h>
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#endif

struct measurement {
    uint64_t pages, relative_pages, standalone_bytes, selected_bytes;
    size_t tail;
};

static bool measure(FILE *base_file, FILE *current_file, struct measurement *result)
{
    uint8_t base[EXODUS_PAGE_BYTES], current[EXODUS_PAGE_BYTES], decoded[EXODUS_PAGE_BYTES];
    uint8_t frame[EXODUS_SNAPSHOT_MAX_BYTES];
    for (;;) {
        size_t base_size = fread(base, 1, sizeof(base), base_file);
        size_t current_size = fread(current, 1, sizeof(current), current_file);
        if (ferror(base_file) || ferror(current_file)) {
            fputs("Could not read both snapshots.\n", stderr);
            return false;
        }
        if (base_size != current_size) {
            fputs("Snapshots must have equal byte lengths.\n", stderr);
            return false;
        }
        /* Bound all counters including frames plus retained base pages. */
        if (result->pages >= UINT64_MAX / (EXODUS_SNAPSHOT_MAX_BYTES + EXODUS_PAGE_BYTES) - 1) {
            fputs("Snapshots exceed the report size limit.\n", stderr);
            return false;
        }
        if (current_size < sizeof(current)) {
            result->tail = current_size;
            result->standalone_bytes += current_size;
            result->selected_bytes += current_size;
            return true;
        }
        size_t selected = exodus_snapshot_encode(current, base, frame);
        bool relative = frame[0] == 1;
        if (!exodus_snapshot_decode(frame, selected, relative ? base : NULL, decoded) ||
            memcmp(current, decoded, sizeof(current)) != 0) {
            fputs("Could not reconstruct the current page.\n", stderr);
            return false;
        }
        ++result->pages;
        result->relative_pages += relative;
        result->selected_bytes += selected;
        result->standalone_bytes += relative ? exodus_snapshot_encode(current, NULL, frame) : selected;
    }
}

#if defined(SNAPSHOT_PROBE_NATIVE_WINDOWS)
typedef wchar_t path_char;
#define PROBE_MAIN wmain
static FILE *open_input(const path_char *path)
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
#else
typedef char path_char;
#define PROBE_MAIN main
static FILE *open_input(const path_char *path)
{
    /* Inspect a writerless FIFO without blocking while opening it. */
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
#endif

int PROBE_MAIN(int argc, path_char **argv)
{
    if (argc != 3) {
        fputs("Usage: page_snapshot_probe REFERENCE CURRENT\n", stderr);
        return EXIT_FAILURE;
    }
    FILE *base = open_input(argv[1]);
    if (!base) {
        fputs("A readable regular snapshot file is required.\n", stderr);
        perror("Reference snapshot");
        return EXIT_FAILURE;
    }
    FILE *current = open_input(argv[2]);
    if (!current) {
        fputs("A readable regular snapshot file is required.\n", stderr);
        perror("Current snapshot");
        fclose(base);
        return EXIT_FAILURE;
    }
    struct measurement stats = {0};
    bool ok = measure(base, current, &stats);
    if (fclose(base) != 0) {
        perror("Closing reference snapshot");
        ok = false;
    }
    if (fclose(current) != 0) {
        perror("Closing current snapshot");
        ok = false;
    }
    if (!ok)
        return EXIT_FAILURE;
    uint64_t retained = stats.relative_pages * EXODUS_PAGE_BYTES;
    if (printf("{\"pages\":%" PRIu64 ",\"relative_pages\":%" PRIu64
               ",\"trailing_bytes\":%zu,\"standalone_frame_bytes\":%" PRIu64
               ",\"selected_frame_bytes\":%" PRIu64 ",\"retained_reference_bytes\":%" PRIu64
               ",\"selected_plus_reference_bytes\":%" PRIu64 "}\n",
               stats.pages, stats.relative_pages, stats.tail, stats.standalone_bytes,
               stats.selected_bytes, retained, stats.selected_bytes + retained) < 0 ||
        fflush(stdout) == EOF) {
        perror("Writing snapshot report");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
