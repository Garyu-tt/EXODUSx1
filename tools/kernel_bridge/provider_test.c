#include "kernel_bridge.h"
#include <assert.h>
#include <stdio.h>
struct mock { unsigned calls; uint32_t result; uint64_t value; };
static uint32_t query(void *ctx,unsigned field,uint64_t *out) {
    struct mock *m=ctx;
    assert(field<EXK_COUNT);
    ++m->calls;
    *out=m->value;
    return m->result;
}
int main(void) {
    struct mock m={0,0,0};
    struct exk_provider p={EXK_ABI_V1,EXK_ALL_FIELDS,query,&m};
    struct exk_value v=exk_query(&p,EXK_PID);
    assert(v.status==EXK_OK && v.native_called && v.value==0);
    m.value=UINT64_C(4294967296);
    v=exk_query(&p,EXK_MEMORY_LIMIT);
    assert(v.status==EXK_OK && v.value==m.value);
    m.result=0x1234;
    v=exk_query(&p,EXK_TID);
    assert(v.status==EXK_NATIVE_ERROR && v.native_called && v.native_result==0x1234 && v.value==0);
    p.capabilities=0;
    v=exk_query(&p,EXK_PID);
    assert(v.status==EXK_UNSUPPORTED && !v.native_called);
    assert(exk_query(&p,EXK_COUNT).status==EXK_INVALID);
    assert(exk_query(NULL,EXK_PID).status==EXK_INVALID);
    p.abi_version=99;
    assert(exk_query(&p,EXK_PID).status==EXK_INVALID);
    p.abi_version=EXK_ABI_V1;
    p.capabilities=UINT32_MAX;
    assert(exk_query(&p,EXK_PID).status==EXK_INVALID);
    p.capabilities=EXK_ALL_FIELDS;
    p.query=NULL;
    assert(exk_query(&p,EXK_PID).status==EXK_INVALID);
    assert(m.calls==3);
    puts("10 provider contract assertions passed");
    return 0;
}
