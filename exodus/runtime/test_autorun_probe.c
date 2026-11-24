#include "autorun_probe.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static const char *const names[] = {
        [EXODUS_AUTORUN_READY] = "ready",
        [EXODUS_AUTORUN_MISSING] = "missing",
        [EXODUS_AUTORUN_UNREADABLE] = "unreadable",
        [EXODUS_AUTORUN_INVALID_NRO] = "invalid-nro",
    };
    if (argc != 2)
        return EXIT_FAILURE;
    puts(names[exodus_autorun_probe(argv[1])]);
    return EXIT_SUCCESS;
}
