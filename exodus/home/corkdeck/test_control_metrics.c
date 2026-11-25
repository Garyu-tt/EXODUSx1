// SPDX-License-Identifier: GPL-2.0-only
#include "control_metrics.h"
#include <assert.h>
int main(void)
{
    char s[64];
    exodus_format_capacity(s, 1048576, 2097152);
    assert(!strcmp(s, "1 / 2 MiB"));
    exodus_format_capacity(s, 0, 0);
    assert(!strcmp(s, "Unavailable"));
    exodus_format_capacity(s, 2, 1);
    assert(!strcmp(s, "Unavailable"));
    exodus_format_capacity(s, UINT64_MAX, UINT64_MAX);
    assert(!strcmp(s, "17592186044415 / 17592186044415 MiB"));
    struct exodus_metrics m;
    exodus_read_metrics(&m);
    assert(!strcmp(m.memory, "Unavailable"));
    assert(!strcmp(m.storage, "Unavailable"));
    assert(!strcmp(m.soc, "Unavailable"));
    assert(!strcmp(m.board, "Unavailable"));
    return 0;
}
