#ifndef EXODUS_LOCAL_MTLS_SMOKE_H
#define EXODUS_LOCAL_MTLS_SMOKE_H
/* Test keys are public. No sockets or production trust. */
#include <string.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include "tls_fixtures.h"

#define EXM_CAP 32768
typedef struct { unsigned char bytes[EXM_CAP]; size_t used; } exm_queue;
typedef struct { exm_queue *in,*out; } exm_bio;
static int exm_send(void *ctx,const unsigned char *buf,size_t n){
    exm_bio *b=ctx;
    size_t room=EXM_CAP-b->out->used;
    if(!room)return MBEDTLS_ERR_SSL_WANT_WRITE;
    if(n>room)n=room;
    memcpy(b->out->bytes+b->out->used,buf,n);b->out->used+=n;
    return (int)n;
}
static int exm_recv(void *ctx,unsigned char *buf,size_t n){
    exm_bio *b=ctx;
    if(!b->in->used)return MBEDTLS_ERR_SSL_WANT_READ;
    if(n>b->in->used)n=b->in->used;
    memcpy(buf,b->in->bytes,n);
    b->in->used-=n;memmove(b->in->bytes,b->in->bytes+n,b->in->used);
    return (int)n;
}
static int exm_retry(int r){return r==MBEDTLS_ERR_SSL_WANT_READ||r==MBEDTLS_ERR_SSL_WANT_WRITE;}
static int exm_config(mbedtls_ssl_config *c,int endpoint,mbedtls_ctr_drbg_context *rng,mbedtls_x509_crt *ca,mbedtls_x509_crt *cert,mbedtls_pk_context *key){
    int r=mbedtls_ssl_config_defaults(c,endpoint,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
    if(r)return r;
    mbedtls_ssl_conf_min_tls_version(c,MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(c,MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_authmode(c,MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_tls13_key_exchange_modes(c,MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
    mbedtls_ssl_conf_rng(c,mbedtls_ctr_drbg_random,rng);
    mbedtls_ssl_conf_ca_chain(c,ca,NULL);
    return cert?mbedtls_ssl_conf_own_cert(c,cert,key):0;
}
static int exm_payload(mbedtls_ssl_context *tx,mbedtls_ssl_context *rx){
    static const unsigned char msg[]="EXODUS LOCAL TLS TEST";
    unsigned char buf[sizeof(msg)]={0};
    size_t sent=0,got=0;
    for(unsigned i=0;i<128 && got<sizeof(msg);++i){
        if(sent<sizeof(msg)){
            int r=mbedtls_ssl_write(tx,msg+sent,sizeof(msg)-sent);
            if(r>0)sent+=(size_t)r;else if(!exm_retry(r))return 0;
        }
        int r=mbedtls_ssl_read(rx,buf+got,sizeof(buf)-got);
        if(r>0)got+=(size_t)r;else if(!exm_retry(r))return 0;
    }
    return sent==sizeof(msg)&&got==sizeof(msg)&&memcmp(msg,buf,sizeof(msg))==0;
}
/* mode=0 valid, 1 missing client cert, 2 wrong server hostname.
 * Return 1 only for expected results, never for configuration errors/timeouts. */
static int exm_case(int mode,mbedtls_ctr_drbg_context *rng,mbedtls_x509_crt *ca,mbedtls_x509_crt *sc,mbedtls_pk_context *sk,mbedtls_x509_crt *cc,mbedtls_pk_context *ck){
    mbedtls_ssl_config a,b;mbedtls_ssl_context s,c;
    mbedtls_ssl_config_init(&a);mbedtls_ssl_config_init(&b);
    mbedtls_ssl_init(&s);mbedtls_ssl_init(&c);
    exm_queue *q=calloc(2,sizeof(*q));int pass=0,r=0,s_done=0,c_done=0;
    if(!q)goto done;
    exm_bio sb={q,q+1},cb={q+1,q};
    if((r=exm_config(&a,MBEDTLS_SSL_IS_SERVER,rng,ca,sc,sk))||
       (r=exm_config(&b,MBEDTLS_SSL_IS_CLIENT,rng,ca,mode==1?NULL:cc,ck))||
       (r=mbedtls_ssl_setup(&s,&a))||(r=mbedtls_ssl_setup(&c,&b))||
       (r=mbedtls_ssl_set_hostname(&c,mode==2?"wrong.invalid":"exodus-smoke.invalid")))goto done;
    mbedtls_ssl_set_bio(&s,&sb,exm_send,exm_recv,NULL);
    mbedtls_ssl_set_bio(&c,&cb,exm_send,exm_recv,NULL);
    for(unsigned i=0;i<256 && !(s_done&&c_done);++i){
        if(!c_done){
            r=mbedtls_ssl_handshake(&c);
            if(!r)c_done=1;
            else if(!exm_retry(r)){
                uint32_t flags=mbedtls_ssl_get_verify_result(&c);
                if(mode==2 && flags==MBEDTLS_X509_BADCERT_CN_MISMATCH)pass=1;
                goto done;
            }
        }
        if(!s_done){
            r=mbedtls_ssl_handshake(&s);
            if(!r)s_done=1;
            else if(!exm_retry(r)){
                if(mode==1 && r==MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE &&
                   mbedtls_ssl_get_verify_result(&s)==MBEDTLS_X509_BADCERT_MISSING)pass=1;
                goto done;
            }
        }
    }
    if(mode==0&&s_done&&c_done&&
       mbedtls_ssl_get_verify_result(&s)==0&&mbedtls_ssl_get_verify_result(&c)==0&&
       mbedtls_ssl_get_peer_cert(&s)&&mbedtls_ssl_get_peer_cert(&c)&&
       strcmp(mbedtls_ssl_get_version(&s),"TLSv1.3")==0&&
       strcmp(mbedtls_ssl_get_version(&c),"TLSv1.3")==0)
        pass=exm_payload(&c,&s)&&exm_payload(&s,&c);
done:
    if(!pass)printf("  mode %d diagnostic rc=%d\n",mode,r);
    mbedtls_ssl_free(&s);mbedtls_ssl_free(&c);
    mbedtls_ssl_config_free(&a);mbedtls_ssl_config_free(&b);
    if(q){mbedtls_platform_zeroize(q,2*sizeof(*q));free(q);}
    return pass;
}
static int exm_run(void){
    int pass=0;
    mbedtls_entropy_context e;mbedtls_ctr_drbg_context rng;
    mbedtls_x509_crt ca,sc,cc;mbedtls_pk_context sk,ck;
    mbedtls_entropy_init(&e);mbedtls_ctr_drbg_init(&rng);
    mbedtls_x509_crt_init(&ca);mbedtls_x509_crt_init(&sc);mbedtls_x509_crt_init(&cc);
    mbedtls_pk_init(&sk);mbedtls_pk_init(&ck);
    static const unsigned char label[]="EXODUS test only";
    int r=mbedtls_ctr_drbg_seed(&rng,mbedtls_entropy_func,&e,label,sizeof(label)-1);
    if(!r)r=mbedtls_x509_crt_parse(&ca,fixture_ca_pem,sizeof(fixture_ca_pem));
    if(!r)r=mbedtls_x509_crt_parse(&sc,fixture_server_pem,sizeof(fixture_server_pem));
    if(!r)r=mbedtls_x509_crt_parse(&cc,fixture_client_pem,sizeof(fixture_client_pem));
    if(!r)r=mbedtls_pk_parse_key(&sk,fixture_server_key,sizeof(fixture_server_key),NULL,0,mbedtls_ctr_drbg_random,&rng);
    if(!r)r=mbedtls_pk_parse_key(&ck,fixture_client_key,sizeof(fixture_client_key),NULL,0,mbedtls_ctr_drbg_random,&rng);
    printf("Test fixture setup: %s (%d)\n",r?"FAIL":"PASS",r);
    if(!r){
        int good=exm_case(0,&rng,&ca,&sc,&sk,&cc,&ck);
        printf("mTLS 1.3 + payload both ways: %s\n",good?"PASS":"FAIL");
        if(good){
            int missing=exm_case(1,&rng,&ca,&sc,&sk,&cc,&ck);
            int hostname=exm_case(2,&rng,&ca,&sc,&sk,&cc,&ck);
            printf("Missing client cert rejection: %s\n",missing?"PASS":"FAIL");
            printf("Wrong hostname rejection: %s\n",hostname?"PASS":"FAIL");
            pass=missing&&hostname;
        }
    }
    mbedtls_pk_free(&ck);mbedtls_pk_free(&sk);
    mbedtls_x509_crt_free(&cc);mbedtls_x509_crt_free(&sc);mbedtls_x509_crt_free(&ca);
    mbedtls_ctr_drbg_free(&rng);mbedtls_entropy_free(&e);
    puts("Public test credentials. Memory BIO only. Remote disabled.");
    return pass;
}
#endif
