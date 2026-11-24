/* Local diagnostic only. No listening socket, pairing or remote grants. */
#include <switch.h>
#include <stdio.h>
#include <time.h>
#include <openssl/rand.h>
#include "horizon_provider.h"
#include "remote_session.h"

static int clock_smoke(void) {
    struct timespec a,b;
    return clock_gettime(CLOCK_MONOTONIC,&a)==0 &&
        clock_gettime(CLOCK_MONOTONIC,&b)==0 &&
        (b.tv_sec>a.tv_sec || (b.tv_sec==a.tv_sec && b.tv_nsec>=a.tv_nsec));
}
static int tls_smoke(void) {
    /* No certificates, persistent credentials or connection established. */
    unsigned char random[32];
    SSL_CTX *ctx=SSL_CTX_new(TLS_server_method());
    int ok=ctx && SSL_CTX_set_min_proto_version(ctx,TLS1_3_VERSION)==1 &&
        SSL_CTX_set_max_proto_version(ctx,TLS1_3_VERSION)==1 &&
        RAND_bytes(random,sizeof(random))==1;
    OPENSSL_cleanse(random,sizeof(random));
    SSL_CTX_free(ctx);
    return ok;
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
    printf("TLS 1.3 context / RNG: %s\n",tls_smoke()?"PASS":"FAIL");
    /* Check deny-by-default with no remote policy admission. */
    exr_provider p=exr_horizon_probe(0);
    exr_session denied={0};
    exw_conn wire={0};
    exs_conn session={0};
    int fail_closed=p.available==0 &&
        exr_dispatch(&denied,&p,0,"system.status",13,out,sizeof(out))==EXR_DENIED &&
        exw_tick(&wire,&denied,0)==EXW_CLOSE &&
        !exs_init(&session,NULL,0,1,0,NULL,0);
    printf("Remote disabled gate: %s\n",fail_closed?"PASS":"FAIL");
    puts("No TLS handshake, socket, or hardware acceptance inferred.");
}
int main(void){
    consoleInit(NULL);
    puts("EXODUS Remote R03.5 - local native smoke");
    printf("OpenSSL: %s\n",OpenSSL_version(OPENSSL_VERSION));
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
