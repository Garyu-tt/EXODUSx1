#ifndef EXODUS_REMOTE_WIRE_H
#define EXODUS_REMOTE_WIRE_H
#include "remote_gate.h"
/* EXR/1 over an already authenticated, encrypted, ordered stream.
 * One state per connection; single-threaded ownership; zero on close.
 * No credentials, permissions or time values may come from the peer. */
#define EXW_HEADER 8u
#define EXW_REQUEST_MAX 32u
#define EXW_REPLY_MAX (EXW_HEADER + EXR_RESPONSE_CAP - 1u)
#define EXW_LIMIT 8u
#define EXW_WINDOW_MS UINT64_C(1000)
#define EXW_FRAME_MS UINT64_C(3000)
typedef struct {
    unsigned char buf[EXW_HEADER + EXW_REQUEST_MAX];
    size_t used, target;
    uint64_t last_ms, start_ms, window_ms;
    unsigned count;
    int closed, started;
} exw_conn;
typedef enum { EXW_MORE=0, EXW_REPLY, EXW_CLOSE } exw_state;
static inline exw_state exw_close(exw_conn *c) {
    memset(c,0,sizeof(*c)); c->closed=1; return EXW_CLOSE;
}
static inline int exw_live(const exr_session *s,uint64_t now) {
    return s && s->authenticated && s->locally_enabled && !s->revoked && now<s->expires_at;
}
/* Invoke on idle timer as well as before each feed. Times are monotonic ms. */
static inline exw_state exw_tick(exw_conn *c,const exr_session *s,uint64_t now) {
    if(c->closed || !exw_live(s,now) || (c->started && now<c->last_ms) ||
       (c->used && now-c->start_ms>=EXW_FRAME_MS)) return exw_close(c);
    if(!c->started){c->started=1;c->window_ms=now;}
    c->last_ms=now; return EXW_MORE;
}
/* Request: 'E','X',1,0, length BE16, reserved BE16=0, exact operation bytes.
 * Reply: same magic/version, status byte, length BE16, reserved=0, payload.
 * Feed consumes at most one frame. Caller loops on remaining input, writes
 * reply completely (with deadline) or closes; no resync on malformed input. */
static inline exw_state exw_feed(exw_conn *c,const exr_session *s,const exr_provider *p,
    uint64_t now,const unsigned char *in,size_t len,size_t *used,
    unsigned char reply[EXW_REPLY_MAX],size_t *reply_len) {
    *used=0; *reply_len=0;
    if(exw_tick(c,s,now)==EXW_CLOSE || (!in && len)) return exw_close(c);
    while(*used<len){
        if(!c->used){c->start_ms=now;c->target=EXW_HEADER;}
        c->buf[c->used++]=in[(*used)++];
        if(c->used==EXW_HEADER){
            unsigned n=((unsigned)c->buf[4]<<8)|c->buf[5];
            if(c->buf[0]!='E'||c->buf[1]!='X'||c->buf[2]!=1||c->buf[3]||c->buf[6]||c->buf[7]||!n||n>EXW_REQUEST_MAX)
                return exw_close(c);
            c->target=EXW_HEADER+n;
        }
        if(c->used!=c->target)continue;
        if(now-c->window_ms>=EXW_WINDOW_MS){c->window_ms=now;c->count=0;}
        if(c->count>=EXW_LIMIT)return exw_close(c);
        ++c->count;
        char out[EXR_RESPONSE_CAP];
        exr_result rc=exr_dispatch(s,p,now,(const char*)c->buf+EXW_HEADER,c->used-EXW_HEADER,out,sizeof(out));
        size_t n=rc==EXR_OK?strlen(out):0;
        reply[0]='E';reply[1]='X';reply[2]=1;reply[3]=(unsigned char)rc;
        reply[4]=(unsigned char)(n>>8);reply[5]=(unsigned char)n;reply[6]=reply[7]=0;
        memcpy(reply+EXW_HEADER,out,n);*reply_len=EXW_HEADER+n;
        memset(c->buf,0,sizeof(c->buf));c->used=c->target=0;
        return EXW_REPLY;
    }
    return EXW_MORE;
}
#endif
