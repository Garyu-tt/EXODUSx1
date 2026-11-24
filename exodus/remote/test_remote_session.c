#include "remote_session.h"
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
static int calls;
static exr_result read_mock(void *ctx,exr_op op,char *out,size_t cap){(void)ctx;(void)op;++calls;snprintf(out,cap,"ok");return EXR_OK;}
static void send_op(SSL *c,const char *op){unsigned char b[40]={'E','X',1,0,0,0,0,0};size_t n=strlen(op);b[5]=(unsigned char)n;memcpy(b+8,op,n);assert(SSL_write(c,b,(int)n+8)==(int)n+8);}
static void receive(SSL *c,int status,const char *payload){unsigned char b[EXW_REPLY_MAX];int n=SSL_read(c,b,sizeof(b));size_t len=strlen(payload);assert(n==(int)(8+len)&&b[3]==status&&b[4]==0&&b[5]==len&&!memcmp(b+8,payload,len));}
int main(void){
 SSL_CTX *sc=ex_tls_server_ctx("ca.pem","server.pem","server.key"),*cc=client_ctx("client.pem","client.key");assert(sc&&cc);
 SSL *s=SSL_new(sc),*c=SSL_new(cc);assert(s&&c&&handshake(s,c));
 ex_tls_peer peer={0};X509 *cert=SSL_get1_peer_certificate(s);unsigned n;assert(cert&&X509_digest(cert,EVP_sha256(),peer.pin,&n)&&n==32);X509_free(cert);
 peer.enabled=1;peer.grants=EXR_BIT(EXR_STATUS)|EXR_BIT(EXR_DISCOVER);
 exr_provider p={EXR_KNOWN,EXR_KNOWN,read_mock,NULL};exs_conn x;
 assert(!exs_init(&x,s,0,10000,0,&peer,1));assert(exs_init(&x,s,0,10000,1,&peer,1));
 #define STEP(t) exs_step(&x,t,1,&peer,1,&p)
 assert(STEP(1)==EXS_WANT_READ);
 send_op(c,"system.status");assert(STEP(2)==EXS_PROGRESS);assert(STEP(3)==EXS_PROGRESS&&calls==1&&x.reply_len);assert(STEP(4)==EXS_PROGRESS);receive(c,EXR_OK,"ok");
 send_op(c,"battery.read");assert(STEP(5)==EXS_PROGRESS);assert(STEP(6)==EXS_PROGRESS);assert(STEP(7)==EXS_PROGRESS);receive(c,EXR_DENIED,"");assert(calls==1);
 send_op(c,"capabilities.list");assert(STEP(8)==EXS_PROGRESS);assert(STEP(9)==EXS_PROGRESS);assert(STEP(10)==EXS_PROGRESS);receive(c,EXR_OK,"capabilities.list\nsystem.status\n");
 send_op(c,"system.status");assert(STEP(11)==EXS_PROGRESS);assert(STEP(12)==EXS_PROGRESS&&x.reply_len);peer.enabled=0;assert(STEP(13)==EXS_CLOSE&&!x.reply_len);assert(SSL_pending(c)==0);assert(STEP(14)==EXS_CLOSE);
 SSL_free(s);SSL_free(c);
 peer.enabled=1;s=SSL_new(sc);c=SSL_new(cc);assert(handshake(s,c));assert(exs_init(&x,s,0,10000,1,&peer,1));send_op(c,"system.status");assert(STEP(1)==EXS_PROGRESS);assert(STEP(2)==EXS_PROGRESS);p.available=0;assert(STEP(3)==EXS_CLOSE&&!x.reply_len);SSL_free(s);SSL_free(c);
 s=SSL_new(sc);c=SSL_new(cc);assert(handshake(s,c));assert(exs_init(&x,s,0,10000,1,&peer,1));assert(STEP(10000)==EXS_CLOSE);SSL_free(s);SSL_free(c);
 SSL_CTX_free(sc);SSL_CTX_free(cc);puts("TLS + EXR session tests passed");
}
