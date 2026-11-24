#ifndef EXODUS_REMOTE_TLS_H
#define EXODUS_REMOTE_TLS_H
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/crypto.h>
#include <stdint.h>
#include <stddef.h>
#define EX_TLS_PIN_LEN 32u
/* Local pairing record; never populate from a remote request. */
typedef struct { unsigned char pin[EX_TLS_PIN_LEN]; uint32_t grants; int enabled; } ex_tls_peer;
/* Explicit trust store only, TLS 1.3, full handshake each connection. */
static inline SSL_CTX *ex_tls_server_ctx(const char *ca, const char *cert, const char *key) {
    if (!ca || !cert || !key) return NULL;
    SSL_CTX *ctx=SSL_CTX_new(TLS_server_method());
    if (!ctx) return NULL;
    if (!SSL_CTX_set_min_proto_version(ctx,TLS1_3_VERSION) ||
        !SSL_CTX_set_max_proto_version(ctx,TLS1_3_VERSION) ||
        !SSL_CTX_load_verify_locations(ctx,ca,NULL) ||
        SSL_CTX_use_certificate_chain_file(ctx,cert)!=1 ||
        SSL_CTX_use_PrivateKey_file(ctx,key,SSL_FILETYPE_PEM)!=1 ||
        SSL_CTX_check_private_key(ctx)!=1 ||
        !SSL_CTX_set_num_tickets(ctx,0) || !SSL_CTX_set_max_early_data(ctx,0)) {
        SSL_CTX_free(ctx); return NULL;
    }
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,NULL);
    SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_OFF);
    return ctx;
}
/* Call on the server after the handshake and before each dispatch/reply.
 * Caller holds current local ACL, monotonic expiry and local enable state.
 * Exact certificate pin; duplicates are rejected, including disabled entries. */
static inline int ex_tls_grants(SSL *ssl,const ex_tls_peer *peers,size_t count,uint32_t *grants) {
    if(!grants)return 0;
    *grants=0;
    if(!ssl || !peers || !count || count>64 || !SSL_is_server(ssl) ||
       !SSL_is_init_finished(ssl) || SSL_version(ssl)!=TLS1_3_VERSION ||
       SSL_session_reused(ssl) || SSL_get_verify_result(ssl)!=X509_V_OK ||
       (SSL_get_verify_mode(ssl)&(SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT))!=
          (SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT)) return 0;
    X509 *cert=SSL_get1_peer_certificate(ssl);
    if(!cert)return 0;
    unsigned char pin[EVP_MAX_MD_SIZE]; unsigned len=0;
    int ok=X509_digest(cert,EVP_sha256(),pin,&len); X509_free(cert);
    if(ok!=1 || len!=EX_TLS_PIN_LEN)return 0;
    size_t matches=0; const ex_tls_peer *match=NULL;
    for(size_t i=0;i<count;i++)if(CRYPTO_memcmp(pin,peers[i].pin,EX_TLS_PIN_LEN)==0){++matches;match=&peers[i];}
    if(matches!=1 || !match->enabled || !match->grants)return 0;
    *grants=match->grants; return 1;
}
#endif
