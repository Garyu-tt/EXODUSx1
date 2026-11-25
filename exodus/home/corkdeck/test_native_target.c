#include "native_target.h"
#include <assert.h>
int main(void)
{
    char id[512];
    assert(exodus_native_target("switch","010000000000ABCD",id)==EXODUS_ROUTE_SWITCH);
    assert(!strcmp(id,"010000000000abcd"));
    assert(exodus_native_target("homebrew","tools/demo.nro",id)==EXODUS_ROUTE_HOMEBREW);
    assert(!strcmp(id,"tools/demo.nro"));
    assert(exodus_native_target("homebrew","sdmc:/Games/My Demo.NRO",id)==EXODUS_ROUTE_HOMEBREW);
    assert(!strcmp(id,"sdmc:/Games/My Demo.NRO"));
    const char *bad[]={"", "../a.nro", "sdmc:/switch/a.nro", "a/../b.nro", "a.nsp", "a\".nro", "a\\b.nro", "a//b.nro", "/a.nro"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        assert(exodus_native_target("homebrew",bad[i],id)==EXODUS_ROUTE_INVALID);
        assert(!id[0]);
    }
    assert(exodus_native_target("switch","0000000000000000",id)==EXODUS_ROUTE_INVALID);
    assert(exodus_native_target("steam","42",id)==EXODUS_ROUTE_INVALID);
    assert(exodus_native_target("other","game.nsp",id)==EXODUS_ROUTE_INVALID);
    assert(exodus_native_target(NULL,"demo.nro",id)==EXODUS_ROUTE_INVALID);
    assert(exodus_native_target("homebrew",NULL,id)==EXODUS_ROUTE_INVALID);
    puts("PASS: native descriptor syntax, canonicalization, provider boundary");
}
