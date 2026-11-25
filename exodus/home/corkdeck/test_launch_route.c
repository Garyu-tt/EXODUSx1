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
    assert(exodus_nro_path("sdmc:/Games/tools/demo.NRO", path));
    assert(!strcmp(path, "sdmc:/Games/tools/demo.NRO"));
    assert(exodus_route_for("Homebrew", "sdmc:/Games/demo.nro") == EXODUS_ROUTE_HOMEBREW);
    assert(!exodus_nro_path("sdmc:/Games/../bad.nro", path));
    assert(!exodus_nro_path("sdmc:/Games//bad.nro", path));
    assert(!exodus_nro_path("sdmc:/GamesExtra/bad.nro", path));
    assert(!exodus_nro_path("sdmc:/Games/bad.nsp", path));
    assert(exodus_route_for("Other", "sdmc:/Games/demo.nro") == EXODUS_ROUTE_UNSUPPORTED);
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
    const struct exodus_execution_target nro={EXODUS_BACKEND_HORIZON,EXODUS_TARGET_NRO};
    const struct exodus_execution_target title={EXODUS_BACKEND_HORIZON,EXODUS_TARGET_TITLE_ID};
    const struct exodus_execution_target bsd={EXODUS_BACKEND_BSD,EXODUS_TARGET_ELF};
    const struct exodus_execution_target wrong={EXODUS_BACKEND_BSD,EXODUS_TARGET_NRO};
    const struct exodus_execution_target steam={EXODUS_BACKEND_STEAM,EXODUS_TARGET_STEAM_APP};
    assert(exodus_backend_route(nro,"test.nro")==EXODUS_ROUTE_HOMEBREW);
    assert(exodus_backend_route(title,"0100000000001234")==EXODUS_ROUTE_SWITCH);
    assert(exodus_backend_route(bsd,"/usr/local/bin/demo")==EXODUS_ROUTE_UNSUPPORTED);
    assert(exodus_backend_route(wrong,"test.nro")==EXODUS_ROUTE_INVALID);
    assert(exodus_backend_route(steam,"42")==EXODUS_ROUTE_UNSUPPORTED);
    assert(!strcmp(exodus_backend_name(nro.backend),"Horizon"));
    assert(!exodus_launch(EXODUS_ROUTE_SWITCH, "0100000000001234", message));
    assert(strstr(message, "unavailable"));
    return 0;
}
