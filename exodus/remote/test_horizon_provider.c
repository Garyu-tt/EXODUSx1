#include "horizon_provider.h"
#include <assert.h>
int main(void) {
    char out[EXR_RESPONSE_CAP];
    exr_provider p = exr_horizon_probe(EXR_KNOWN);
    assert(p.reviewed == EXR_KNOWN);
#ifndef __SWITCH__
    assert(p.available == EXR_BIT(EXR_DISCOVER));
    assert(p.read(NULL, EXR_STATUS, out, sizeof(out)) == EXR_UNAVAILABLE);
    assert(!out[0]);
#else
    assert(p.available == EXR_KNOWN);
    assert(psm_opens == psm_closes && fs_opens == fs_closes);
    assert(p.read(NULL, EXR_STATUS, out, sizeof(out)) == EXR_OK && strstr(out,"scope=current_process"));
    assert(p.read(NULL, EXR_BATTERY, out, sizeof(out)) == EXR_OK && strstr(out,"charge_percent=75"));
    assert(p.read(NULL, EXR_STORAGE, out, sizeof(out)) == EXR_OK && strstr(out,"free_bytes=500"));
    assert(p.read(NULL, EXR_STORAGE, out, 2) == EXR_PROVIDER_ERROR && !out[0]);
    fail_reads = 1;
    p = exr_horizon_probe(EXR_KNOWN);
    assert(p.available == EXR_BIT(EXR_DISCOVER));
    assert(psm_opens == psm_closes && fs_opens == fs_closes);
    fail_reads = 0;
    unsigned before = psm_opens + fs_opens;
    p = exr_horizon_probe(EXR_BIT(EXR_STATUS));
    assert(p.available == EXR_BIT(EXR_STATUS) && psm_opens + fs_opens == before);
#endif
    assert(p.read(NULL, EXR_STATUS, out, 0) == EXR_BAD_REQUEST);
    puts("provider tests passed");
}
