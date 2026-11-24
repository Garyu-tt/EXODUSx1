/* Local diagnostic only: no listener, credentials or remote grants. */
#include <switch.h>
#include <stdio.h>
#include <mbedtls/entropy.h>
#include <mbedtls/entropy_poll.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
#include <mbedtls/platform_util.h>
#include <psa/crypto.h>
#if !defined(MBEDTLS_SSL_PROTO_TLS1_3) || !defined(MBEDTLS_ENTROPY_HARDWARE_ALT) || !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
#error Required TLS 1.3 and entropy configuration missing
#endif
#if defined(MBEDTLS_SSL_SESSION_TICKETS) || defined(MBEDTLS_SSL_TICKET_C) || \
    defined(MBEDTLS_SSL_CACHE_C) || defined(MBEDTLS_SSL_EARLY_DATA) || \
    defined(MBEDTLS_SSL_CONTEXT_SERIALIZATION) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED) || \
    defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) || defined(MBEDTLS_PSA_ITS_FILE_C)
#error Unexpected session resumption or persistent storage configuration
#endif
#if !defined(MBEDTLS_HAVE_TIME_DATE) || !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE) || \
    !defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#error Required certificate/time configuration missing
#endif
int mbedtls_hardware_poll(void *ctx,unsigned char *out,size_t len,size_t *olen){
    (void)ctx;
    if(!olen || (!out && len))return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *olen=0;
    if(!len)return 0;
    if(R_FAILED(csrngInitialize())){mbedtls_platform_zeroize(out,len);return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}
    Result rc=csrngGetRandomBytes(out,len);
    csrngExit();
    if(R_FAILED(rc)){mbedtls_platform_zeroize(out,len);return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}
    *olen=len;return 0;
}
static void run(void){
    unsigned char random[32]={0};size_t n=0;
    int rc=mbedtls_hardware_poll(NULL,random,sizeof(random),&n);
    mbedtls_platform_zeroize(random,sizeof(random));
    printf("CSRNG IPC: %s\n",rc==0&&n==32?"PASS":"FAIL");
    if(rc||n!=32)return;
    psa_status_t ps=psa_crypto_init();
    printf("PSA init: %s\n",ps==PSA_SUCCESS?"PASS":"FAIL");
    if(ps!=PSA_SUCCESS)return;
    mbedtls_entropy_context e;mbedtls_entropy_init(&e);
    rc=mbedtls_entropy_func(&e,random,sizeof(random));
    mbedtls_platform_zeroize(random,sizeof(random));mbedtls_entropy_free(&e);
    printf("Entropy accumulator: %s\n",rc==0?"PASS":"FAIL");
    mbedtls_ssl_config c;mbedtls_ssl_config_init(&c);
    rc=mbedtls_ssl_config_defaults(&c,MBEDTLS_SSL_IS_SERVER,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
    if(!rc){
        mbedtls_ssl_conf_min_tls_version(&c,MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_max_tls_version(&c,MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_authmode(&c,MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_tls13_key_exchange_modes(&c,MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
    }
    printf("TLS 1.3 config only: %s\n",rc==0?"PASS":"FAIL");
    mbedtls_ssl_config_free(&c);
    puts("No certificate, handshake or network test. Remote disabled.");
}
int main(void){
    consoleInit(NULL);
    char v[64];mbedtls_version_get_string_full(v);
    printf("EXODUS R03.7 %s\nA: local test; Plus: exit\n",v);
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
    while(appletMainLoop()){padUpdate(&pad);u64 down=padGetButtonsDown(&pad);
        if(down&HidNpadButton_Plus)break;
        if(down&HidNpadButton_A)run();
        consoleUpdate(NULL);
    }
    consoleExit(NULL);return 0;
}
