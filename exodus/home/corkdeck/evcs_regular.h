/* SPDX-License-Identifier: GPL-2.0-only
 * Admit a read-only regular file matching a preceding lstat observation.
 * Owner-controlled parent directories are assumed. No filesystem writes. */
#ifndef EXODUS_EVCS_REGULAR_H
#define EXODUS_EVCS_REGULAR_H
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static inline FILE *evcs_open_observed_regular(const char *path, const struct stat *before)
{
    if (!path || !before || !S_ISREG(before->st_mode) || before->st_size<=0) {
        errno=EINVAL; return NULL;
    }
    int flags=O_RDONLY|O_NONBLOCK;
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
    int fd=open(path,flags);
    if (fd<0) return NULL;
    struct stat actual, current;
    if (fstat(fd,&actual) || lstat(path,&current) ||
        !S_ISREG(actual.st_mode) || !S_ISREG(current.st_mode) ||
        actual.st_dev!=before->st_dev || actual.st_ino!=before->st_ino ||
        actual.st_size!=before->st_size ||
        current.st_dev!=actual.st_dev || current.st_ino!=actual.st_ino ||
        current.st_size!=actual.st_size) {
        close(fd); errno=EINVAL; return NULL;
    }
    FILE *stream=fdopen(fd,"rb");
    if (!stream) { int err=errno; close(fd); errno=err; }
    return stream;
}
#endif
