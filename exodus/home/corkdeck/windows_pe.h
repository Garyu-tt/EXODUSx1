// SPDX-License-Identifier: GPL-2.0-only
#ifndef EXODUS_WINDOWS_PE_H
#define EXODUS_WINDOWS_PE_H

#include <stdio.h>

enum exodus_windows_pe_status {
    EXODUS_WINDOWS_PE_INVALID,
    EXODUS_WINDOWS_PE_UNSUPPORTED_MACHINE,
    EXODUS_WINDOWS_PE_READY,
};

/* Check a seekable PE application image against CorkDeck's supported machine/format pairs. */
enum exodus_windows_pe_status exodus_windows_pe_probe(FILE *image);

#endif
