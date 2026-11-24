// SPDX-License-Identifier: GPL-2.0-only
#include "../../runtime/autorun_probe.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s SHELL.nro\n", argv[0]);
        return 2;
    }
    enum exodus_autorun_status status = exodus_autorun_probe(argv[1]);
    if (status != EXODUS_AUTORUN_READY) {
        fprintf(stderr, "NRO preflight failed: %d\n", (int)status);
        return 1;
    }
    puts("NRO structure passed; execution not verified.");
    return 0;
}
