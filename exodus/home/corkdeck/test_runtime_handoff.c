#include "runtime_handoff.h"
#include <assert.h>
int main(void)
{
    char args[EXODUS_RUNTIME_ARGS_CAP];
    assert(exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "sdmc:/Games/My Game.EXE", args));
    assert(!strcmp(args, EXODUS_AUTORUN_PATH " \"sdmc:/Games/My Game.EXE\""));
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "sdmc:/Games/../bad.exe", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "sdmc:/Games/a\"b.exe", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "sdmc:/Games/a\\b.exe", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "sdmc:/Games//a.exe", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "ums0:/Wine/a.exe", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, "sdmc:/Games/a.nro", args));
    assert(exodus_runtime_args(EXODUS_ROUTE_STEAM, "4294967295", args));
    assert(!strcmp(args, EXODUS_AUTORUN_PATH));
    assert(!exodus_runtime_args(EXODUS_ROUTE_STEAM, "4294967296", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_STEAM, "0", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_STEAM, "42;other", args));
    assert(!exodus_runtime_args(EXODUS_ROUTE_SWITCH, "42", args));
    char long_path[512];
    memset(long_path, 'a', sizeof(long_path));
    memcpy(long_path, "sdmc:/", 6);
    memcpy(long_path + sizeof(long_path) - 5, ".exe", 5);
    assert(!exodus_runtime_args(EXODUS_ROUTE_WINDOWS, long_path, args));
    return 0;
}
