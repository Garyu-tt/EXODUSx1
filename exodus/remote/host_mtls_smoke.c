#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/platform_util.h>
#include <psa/crypto.h>
#include "local_mtls_smoke.h"
int main(void){
    if(psa_crypto_init()!=PSA_SUCCESS)return 2;
    return exm_run()?0:1;
}
