/* EXODUS Kernel Bridge R02: own-process, read-only admission probe. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

typedef uint32_t (*query_fn)(void *, unsigned, uint64_t *);
enum { PID, TID, TOTAL, USED, COUNT };
static const char *const names[COUNT] = {"process_id","thread_id","memory_limit_bytes","memory_used_bytes"};

struct observation { uint32_t result; uint64_t value; };
static void observe(query_fn query, void *ctx, struct observation out[COUNT]) {
    for (unsigned i=0; i<COUNT; ++i) {
        uint64_t value=0;
        out[i].result=query(ctx,i,&value);
        out[i].value=out[i].result == 0 ? value : 0;
    }
}
static int report(FILE *f, const char *environment, const struct observation out[COUNT]) {
    unsigned ok=0;
    if (fprintf(f,"schema=exodus.admission.v1 environment=%s\n",environment)<0) return -1;
    for (unsigned i=0; i<COUNT; ++i) {
        int rc;
        if (out[i].result == 0) {
            ++ok;
            rc=fprintf(f,"%s status=observed result=0x%08" PRIx32 " value=%" PRIu64 "\n",
                       names[i],out[i].result,out[i].value);
        } else {
            rc=fprintf(f,"%s status=failed result=0x%08" PRIx32 " value=unknown\n",
                       names[i],out[i].result);
        }
        if (rc<0) return -1;
    }
    /* Separate sequential snapshots: no guaranteed atomic memory accounting. */
    if (fprintf(f,"observed=%u/%u full_desktop_ready=unknown\n",ok,(unsigned)COUNT)<0) return -1;
    return fflush(f)==0 ? 0 : -1;
}

#ifdef __SWITCH__
static uint32_t horizon_query(void *ctx, unsigned field, uint64_t *out) {
    (void)ctx;
    switch (field) {
        case PID: return svcGetProcessId(out,CUR_PROCESS_HANDLE);
        case TID: return svcGetThreadId(out,CUR_THREAD_HANDLE);
        case TOTAL: return svcGetInfo(out,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
        case USED: return svcGetInfo(out,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
        default: return UINT32_MAX;
    }
}
int main(void) {
    struct observation out[COUNT];
    consoleInit(NULL);
    printf("HOS version encoded=0x%08" PRIx32 " applet_type=%d\n",
           (uint32_t)hosversionGet(),(int)appletGetAppletType());
    observe(horizon_query,NULL,out);
    const int rc=report(stdout,"horizon",out);
    puts("Plus: exit");
    PadState pad;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad)&HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return rc!=0;
}
#else
#include <assert.h>
#include <string.h>
struct mock { unsigned calls; uint32_t failure_mask; };
static uint32_t mock_query(void *ctx,unsigned field,uint64_t *out) {
    struct mock *m=ctx;
    ++m->calls;
    *out=field==TOTAL ? UINT64_C(4294967296) : field==USED ? 0 : 100+field;
    return m->failure_mask&(1u<<field) ? 0x1234u : 0u;
}
int main(void) {
    struct observation out[COUNT];
    struct mock m={0,0};
    observe(mock_query,&m,out);
    assert(m.calls==COUNT);
    assert(out[TOTAL].value==UINT64_C(4294967296));
    assert(out[USED].value==0 && out[USED].result==0);
    m.failure_mask=(1u<<PID)|(1u<<USED);
    observe(mock_query,&m,out);
    assert(m.calls==2*COUNT);
    assert(out[PID].result==0x1234 && out[PID].value==0);
    assert(out[USED].result==0x1234 && out[USED].value==0);
    FILE *f=tmpfile();
    assert(f);
    assert(report(f,"host_mock",out)==0);
    rewind(f);
    char buf[1024]={0};
    size_t n=fread(buf,1,sizeof(buf)-1,f);
    assert(n>0 && !ferror(f));
    assert(strstr(buf,"environment=host_mock"));
    assert(strstr(buf,"process_id status=failed result=0x00001234 value=unknown"));
    assert(strstr(buf,"memory_limit_bytes status=observed result=0x00000000 value=4294967296"));
    assert(strstr(buf,"observed=2/4 full_desktop_ready=unknown"));
    fclose(f);
    puts("host mock assertions passed");
    return 0;
}
#endif
