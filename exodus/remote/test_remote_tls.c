#include "remote_tls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SSL_CTX *client_ctx(const char *cert,const char *key){
 SSL_CTX *c=SSL_CTX_new(TLS_client_method());assert(c);
 assert(SSL_CTX_set_min_proto_version(c,TLS1_3_VERSION));assert(SSL_CTX_load_verify_locations(c,"ca.pem",NULL));
 SSL_CTX_set_verify(c,SSL_VERIFY_PEER,NULL);
 if(cert){assert(SSL_CTX_use_certificate_chain_file(c,cert)==1);assert(SSL_CTX_use_PrivateKey_file(c,key,SSL_FILETYPE_PEM)==1);}return c;
}
static int handshake(SSL *s,SSL *c){
 BIO *a,*b;assert(BIO_new_bio_pair(&a,0,&b,0));SSL_set_bio(s,a,a);SSL_set_bio(c,b,b);SSL_set_accept_state(s);SSL_set_connect_state(c);assert(SSL_set1_host(c,"localhost"));
 for(int i=0;i<100;i++){
  SSL *x[2]={c,s};for(int j=0;j<2;j++)if(!SSL_is_init_finished(x[j])){int n=SSL_do_handshake(x[j]);if(n!=1){int e=SSL_get_error(x[j],n);if(e!=SSL_ERROR_WANT_READ&&e!=SSL_ERROR_WANT_WRITE)return 0;}}
  if(SSL_is_init_finished(s)&&SSL_is_init_finished(c))return 1;
 }return 0;
}
int main(void){
 SSL_CTX *sc=ex_tls_server_ctx("ca.pem","server.pem","server.key");assert(sc);assert(!ex_tls_server_ctx("missing.pem","server.pem","server.key"));
 SSL_CTX *cc=client_ctx("client.pem","client.key");SSL *s=SSL_new(sc),*c=SSL_new(cc);assert(s&&c);
 ex_tls_peer p[2]={0};uint32_t g=99;assert(!ex_tls_grants(s,p,1,&g)&&g==0);assert(handshake(s,c));
 assert(!ex_tls_grants(s,p,1,&g)&&g==0);X509 *cert=SSL_get1_peer_certificate(s);unsigned n;assert(cert);assert(X509_digest(cert,EVP_sha256(),p[0].pin,&n)&&n==32);X509_free(cert);
 p[0].enabled=1;p[0].grants=3;assert(ex_tls_grants(s,p,1,&g)&&g==3);p[0].enabled=0;assert(!ex_tls_grants(s,p,1,&g)&&g==0);p[0].enabled=1;p[1]=p[0];assert(!ex_tls_grants(s,p,2,&g));assert(!ex_tls_grants(c,p,1,&g));
 assert(SSL_write(c,"hello",5)==5);char buf[8];assert(SSL_read(s,buf,sizeof(buf))==5&&!memcmp(buf,"hello",5));SSL_free(s);SSL_free(c);SSL_CTX_free(cc);
 cc=client_ctx(NULL,NULL);s=SSL_new(sc);c=SSL_new(cc);assert(!handshake(s,c));SSL_free(s);SSL_free(c);SSL_CTX_free(cc);
 cc=client_ctx("rogue.pem","rogue.key");s=SSL_new(sc);c=SSL_new(cc);assert(!handshake(s,c));SSL_free(s);SSL_free(c);SSL_CTX_free(cc);SSL_CTX_free(sc);puts("TLS 1.3 mutual authentication and local ACL tests passed");
}
