#include "remote_gate.h"
#include <stdio.h>
#include <assert.h>
static int calls;
static exr_result read_mock(void *ctx, exr_op op, char *out, size_t cap) {
    (void)ctx; ++calls;
    if (op == EXR_STORAGE) { memset(out, 'x', cap); return EXR_OK; }
    if (op == EXR_BATTERY) { snprintf(out, cap, "partial"); return EXR_UNAVAILABLE; }
    snprintf(out, cap, "mock status"); return EXR_OK;
}
#define RUN(name) exr_dispatch(&s, &p, 10, name, sizeof(name)-1, out, sizeof(out))
int main(void) {
    exr_session s = {1,1,0,20,EXR_KNOWN};
    exr_provider p = {EXR_KNOWN, EXR_KNOWN, read_mock, NULL};
    char out[EXR_RESPONSE_CAP];
    assert(RUN("capabilities.list") == EXR_OK && strstr(out,"system.status"));
    s.grants &= ~EXR_BIT(EXR_BATTERY);
    assert(RUN("capabilities.list") == EXR_OK && !strstr(out,"battery.read"));
    assert(RUN("battery.read") == EXR_DENIED && !out[0] && calls==0);
    assert(RUN("system.status") == EXR_OK && !strcmp(out,"mock status") && calls==1);
    assert(RUN("system.status\0extra") == EXR_BAD_REQUEST && !out[0]);
    assert(RUN("system.status\n") == EXR_BAD_REQUEST);
    assert(RUN("shell.exec") == EXR_BAD_REQUEST);
    assert(RUN("nintendo.account") == EXR_BAD_REQUEST);
    s.authenticated=0; assert(RUN("system.status")==EXR_DENIED); s.authenticated=1;
    s.locally_enabled=0; assert(RUN("system.status")==EXR_DENIED); s.locally_enabled=1;
    s.revoked=1; assert(RUN("system.status")==EXR_DENIED); s.revoked=0;
    s.expires_at=10; assert(RUN("system.status")==EXR_DENIED); s.expires_at=20;
    p.reviewed &= ~EXR_BIT(EXR_STATUS); assert(RUN("system.status")==EXR_UNAVAILABLE);
    p.reviewed=EXR_KNOWN; p.available &= ~EXR_BIT(EXR_STATUS);
    assert(RUN("system.status")==EXR_UNAVAILABLE && calls==1);
    p.available=EXR_KNOWN; s.grants=EXR_KNOWN;
    assert(RUN("storage.read")==EXR_PROVIDER_ERROR && !out[0]);
    assert(RUN("battery.read")==EXR_PROVIDER_ERROR && !out[0]);
    p.read=NULL; assert(RUN("capabilities.list")==EXR_OK && !strstr(out,"system.status"));
    assert(RUN("system.status")==EXR_UNAVAILABLE);
    char tiny[2]; assert(exr_dispatch(&s,&p,10,"capabilities.list",17,tiny,sizeof(tiny))==EXR_PROVIDER_ERROR && !tiny[0]);
    assert(exr_dispatch(&s,&p,10,NULL,0,out,sizeof(out))==EXR_BAD_REQUEST);
    puts("EXODUS remote gate: all assertions passed (mock providers only)");
}
