#ifndef EXODUS_REMOTE_SESSION_H
#define EXODUS_REMOTE_SESSION_H
#include "remote_tls.h"
#include "remote_wire.h"
#include <openssl/err.h>
#define EXS_WRITE_MS UINT64_C(3000)
typedef enum { EXS_PROGRESS, EXS_WANT_READ, EXS_WANT_WRITE, EXS_CLOSE } exs_state;
typedef struct {
    SSL *tls; /* borrowed; caller must abort/free on CLOSE */
    exw_conn wire;
    uint64_t expires, reply_since;
    uint32_t grants, reviewed, available;
    unsigned char input[EXW_HEADER+EXW_REQUEST_MAX], reply[EXW_REPLY_MAX];
    size_t reply_len, sent;
    int closed;
} exs_conn;
static inline exs_state exs_close(exs_conn *c) {
    OPENSSL_cleanse(c,sizeof(*c)); c->closed=1; return EXS_CLOSE;
}
/* Initialize only after a verified handshake, with local monotonic expiry.
 * Caller owns SSL, uses nonblocking BIOs, serializes policy and provider state. */
static inline int exs_init(exs_conn *c,SSL *tls,uint64_t now,uint64_t expires,
                           int enabled,const ex_tls_peer *peers,size_t count) {
    memset(c,0,sizeof(*c)); uint32_t g=0;
    if(!enabled || now>=expires || !ex_tls_grants(tls,peers,count,&g) || !(g&EXR_KNOWN)) {
        exs_close(c); return 0;
    }
    c->tls=tls;c->expires=expires;c->grants=g&EXR_KNOWN;
    exr_session s={1,1,0,expires,c->grants};
    exw_tick(&c->wire,&s,now);return 1;
}
static inline exs_state exs_io_result(exs_conn *c,int rc) {
    int err=SSL_get_error(c->tls,rc);
    if(err==SSL_ERROR_WANT_READ)return EXS_WANT_READ;
    if(err==SSL_ERROR_WANT_WRITE)return EXS_WANT_WRITE;
    return exs_close(c);
}
/* One bounded TLS IO attempt. Recheck ACL on EVERY step, including queued writes.
 * Grant changes close the connection; fresh pairing policy requires reconnect.
 * Caller supplies fresh monotonic time, local enable, ACL and provider snapshot.
 * On close do not flush pending TLS output or call SSL_shutdown. */
static inline exs_state exs_step(exs_conn *c,uint64_t now,int enabled,
      const ex_tls_peer *peers,size_t count,const exr_provider *p) {
    uint32_t g=0;
    if(c->closed || !enabled || !p || !ex_tls_grants(c->tls,peers,count,&g) ||
       (g&EXR_KNOWN)!=c->grants)return exs_close(c);
    exr_session s={1,1,0,c->expires,c->grants};
    if(exw_tick(&c->wire,&s,now)==EXW_CLOSE)return exs_close(c);
    size_t n=0;
    if(c->reply_len){
        if(now-c->reply_since>=EXS_WRITE_MS || p->reviewed!=c->reviewed || p->available!=c->available)
            return exs_close(c);
        ERR_clear_error();
        int rc=SSL_write_ex(c->tls,c->reply+c->sent,c->reply_len-c->sent,&n);
        if(rc!=1)return exs_io_result(c,rc);
        if(!n || n>c->reply_len-c->sent)return exs_close(c);
        c->sent+=n;
        if(c->sent==c->reply_len){OPENSSL_cleanse(c->reply,sizeof(c->reply));c->sent=c->reply_len=0;}
        return EXS_PROGRESS;
    }
    size_t want=c->wire.used?c->wire.target-c->wire.used:EXW_HEADER;
    if(!want || want>sizeof(c->input))return exs_close(c);
    ERR_clear_error();
    int rc=SSL_read_ex(c->tls,c->input,want,&n);
    if(rc!=1)return exs_io_result(c,rc);
    if(!n || n>want)return exs_close(c);
    size_t used=0;
    exw_state state=exw_feed(&c->wire,&s,p,now,c->input,n,&used,c->reply,&c->reply_len);
    OPENSSL_cleanse(c->input,sizeof(c->input));
    if(state==EXW_CLOSE || used!=n)return exs_close(c);
    if(state==EXW_REPLY){c->reply_since=now;c->reviewed=p->reviewed;c->available=p->available;}
    return EXS_PROGRESS;
}
#endif
