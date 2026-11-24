#include "horizon_settings.h"
#include <assert.h>

int main(void)
{
    struct exodus_settings s;
    memset(&s, 0xff, sizeof(s));
    exodus_read_settings(&s);
    assert(!strcmp(s.firmware, "Unavailable"));
    assert(!strcmp(s.battery, "Unavailable"));
    assert(!strcmp(s.network, "Unavailable"));
    assert(!strcmp(s.wifi, "Unavailable"));
    assert(!strcmp(s.bluetooth, "Unavailable"));
    assert(!strcmp(s.brightness, "Unavailable"));
    assert(!strcmp(s.performance, "Unavailable"));
    return 0;
}
