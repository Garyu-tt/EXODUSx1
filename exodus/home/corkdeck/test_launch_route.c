// SPDX-License-Identifier: GPL-2.0-only
#include "native_launch.h"
#include <assert.h>
int main(void)
{
    uint64_t id;
    char path[512], message[256];
    assert(exodus_title_id("0100000000001234", &id) && id == UINT64_C(0x0100000000001234));
    assert(!exodus_title_id("0000000000000000", &id));
    assert(!exodus_title_id("010000000000123z", &id));
    assert(exodus_nro_path("tools/example.nro", path));
    assert(!strcmp(path, "sdmc:/switch/tools/example.nro"));
    assert(!exodus_nro_path("../bad.nro", path));
    assert(!exodus_nro_path("tools//bad.nro", path));
    assert(!exodus_nro_path("sdmc:/bad.nro", path));
    assert(!exodus_nro_path("tools/./bad.nro", path));
    assert(!exodus_nro_path("tools/bad.exe", path));
    assert(exodus_route_for("Switch", "0100000000001234") == EXODUS_ROUTE_SWITCH);
    assert(exodus_route_for("Homebrew", "test.nro") == EXODUS_ROUTE_HOMEBREW);
    assert(exodus_route_for("CorkDeck", "x.exe") == EXODUS_ROUTE_WINDOWS);
    assert(exodus_route_for("Steam", "42") == EXODUS_ROUTE_STEAM);
    assert(exodus_route_for("Other", "42") == EXODUS_ROUTE_UNSUPPORTED);
    assert(!exodus_launch(EXODUS_ROUTE_SWITCH, "0100000000001234", message));
    assert(strstr(message, "unavailable"));
    return 0;
}
