// SPDX-License-Identifier: GPL-2.0-only
#include "native_launch.h"
#include <assert.h>

static void check_homebrew_paths(void)
{
    const char *valid[] = {
        "Tools/Café Demo.nro", "Tools/Café Demo.nrO",
        "Tools/Café Demo.nRo", "Tools/Café Demo.nRO",
        "Tools/Café Demo.Nro", "Tools/Café Demo.NrO",
        "Tools/Café Demo.NRo", "Tools/Café Demo.NRO"
    };
    char path[512], expected[512];
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        assert(exodus_nro_path(valid[i], path));
        snprintf(expected, sizeof(expected), "sdmc:/switch/%s", valid[i]);
        assert(!strcmp(path, expected));
        assert(exodus_route_for("Homebrew", valid[i]) == EXODUS_ROUTE_HOMEBREW);
    }

    const char *invalid[] = {
        "", ".NRO", "DemoNRO", "Demo.NR", "Demo.NR0", "Demo.NRO.exe",
        "Demo.NRO ", "Demo.NRO/", "Demo.nr\xd0\xbe", "/Demo.NRO",
        "../Demo.NRO", "Tools/../Demo.NRO", "Tools/./Demo.NRO",
        "Tools//Demo.NRO", "sdmc:/Demo.NRO", "Tools\\Demo.NRO",
        "Tools/\"Demo.NRO", "Tools/\nDemo.NRO", "Tools/\x7f" "Demo.NRO"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(!exodus_nro_path(invalid[i], path));
        assert(exodus_route_for("Homebrew", invalid[i]) == EXODUS_ROUTE_INVALID);
    }
    assert(!exodus_nro_path(NULL, path));

    /* Preserve the 498-byte ID limit and all original path bytes. */
    char longest[500];
    memset(longest, 'a', 245);
    longest[245] = '/';
    memset(longest + 246, 'B', 248);
    memcpy(longest + 494, ".NRO", 5);
    assert(exodus_nro_path(longest, path));
    assert(strlen(path) == sizeof(path) - 1);
    assert(!strcmp(path + strlen("sdmc:/switch/"), longest));
    longest[494] = 'B';
    memcpy(longest + 495, ".NRO", 5);
    assert(!exodus_nro_path(longest, path));
    assert(exodus_route_for("Homebrew", longest) == EXODUS_ROUTE_INVALID);
}

int main(void)
{
    check_homebrew_paths();
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
