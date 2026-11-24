/* Local diagnostic only. No listening socket, pairing or remote grants. */
#include <switch.h>
#include <stdio.h>
#include <time.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/version.h>
#include <mbedtls/ssl.h>
#include "horizon_provider.h"
#include "remote_wire.h"
#include "horizon_socket_smoke.h"

static int clock_smoke(void) {
    struct timespec a,b;
    return clock_gettime(CLOCK_MONOTONIC,&a)==0 &&
        clock_gettime(CLOCK_MONOTONIC,&b)==0 &&
        (b.tv_sec>a.tv_sec || (b.tv_sec==a.tv_sec && b.tv_nsec>=a.tv_nsec));
}
static int entropy_smoke(void) {
    unsigned char random[32]={0};
    Result rc=csrngInitialize();
    if(R_FAILED(rc))return 0;
    rc=csrngGetRandomBytes(random,sizeof(random));
    csrngExit();
    mbedtls_platform_zeroize(random,sizeof(random));
    return R_SUCCEEDED(rc);
}
static void local_probe(void) {
    char out[EXR_RESPONSE_CAP];
    puts("Local diagnostic; no remote access enabled.");
    for(exr_op op=EXR_STATUS;op<EXR_COUNT;++op){
        exr_result rc=exr_horizon_read(NULL,op,out,sizeof(out));
        printf("%s: %s\n",exr_name(op),rc==EXR_OK?"observed":"unavailable");
        if(rc==EXR_OK)puts(out);
    }
    printf("Monotonic clock: %s\n",clock_smoke()?"PASS":"FAIL");
    printf("Horizon CSRNG IPC: %s\n",entropy_smoke()?"PASS":"FAIL");
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    puts("TLS 1.3 configured: yes (handshake not tested)");
#else
    puts("TLS 1.3 configured: NO - remote transport blocked");
#endif
    /* Check deny-by-default with no remote policy admission. */
    exr_provider p=exr_horizon_probe(0);
    exr_session denied={0};
    exw_conn wire={0};
    int fail_closed=p.available==0 &&
        exr_dispatch(&denied,&p,0,"system.status",13,out,sizeof(out))==EXR_DENIED &&
        exw_tick(&wire,&denied,0)==EXW_CLOSE;
    printf("Remote disabled gate: %s\n",fail_closed?"PASS":"FAIL");
    int net=exn_loopback_smoke();
    printf("Native TCP loopback: %s (stage=%d)\n",net==0?"PASS":"FAIL",net);
    puts("Loopback only; no TLS handshake or remote access.");
}
int main(void){
    consoleInit(NULL);
    puts("EXODUS Remote R03.14 - local native smoke");
    char version[64]; mbedtls_version_get_string_full(version);
    printf("Crypto diagnostic: %s\n",version);
    puts("Press A to run local read-only diagnostics; Plus to exit.");
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad;padInitializeDefault(&pad);
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 down=padGetButtonsDown(&pad);
        if(down&HidNpadButton_Plus)break;
        if(down&HidNpadButton_A)local_probe();
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
