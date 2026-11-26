/* AMP control-plane contract, not a FreeBSD syscall implementation.
 * Byte encoding independent of C ABI. No raw pointers, arbitrary syscall
 * numbers, hardware access, or claims about physical cache coherence.
 * One outstanding request per trusted serialized session. */
#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#endif
enum { AMP_SIZE=64, AMP_REQ=1, AMP_REPLY=2, AMP_GETPID=1, AMP_EXIT_INTENT=2,
       AMP_OK=0, AMP_DENIED=1 };
struct amp_msg { uint8_t kind,op,status; uint64_t generation,seq,task,arg,result; };
struct amp_session { uint64_t generation,task,last,pending_arg; uint8_t pending,closed; };
static void put(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++){p[i]=(uint8_t)v;v>>=8;}}
static uint64_t get(const uint8_t *p){uint64_t v=0;for(unsigned i=8;i;i--)v=(v<<8)|p[i-1];return v;}
static int valid(const struct amp_msg *m){
    if(!m||!m->generation||!m->seq||!m->task)return 0;
    if(m->op!=AMP_GETPID&&m->op!=AMP_EXIT_INTENT)return 0;
    if((m->op==AMP_GETPID&&m->arg)||(m->op==AMP_EXIT_INTENT&&m->arg>255))return 0;
    if(m->kind==AMP_REQ)return !m->status&&!m->result;
    if(m->kind!=AMP_REPLY||m->status>AMP_DENIED)return 0;
    if(m->status==AMP_DENIED)return !m->result;
    return m->op==AMP_GETPID?m->result>0&&m->result<=UINT32_C(0x7fffffff):!m->result;
}
static int encode(uint8_t *out,size_t len,const struct amp_msg *m){
    if(!out||len!=AMP_SIZE||!valid(m))return 0;
    memset(out,0,len);memcpy(out,"AMP2",4);out[4]=2;out[5]=m->kind;out[6]=m->op;out[7]=m->status;
    put(out+8,m->generation);put(out+16,m->seq);put(out+24,m->task);put(out+32,m->arg);put(out+40,m->result);
    return 1;
}
static int decode(struct amp_msg *m,const uint8_t *in,size_t len){
    if(!m||!in||len!=AMP_SIZE||memcmp(in,"AMP2",4)||in[4]!=2)return 0;
    for(unsigned i=48;i<AMP_SIZE;i++)if(in[i])return 0;
    struct amp_msg t={in[5],in[6],in[7],get(in+8),get(in+16),get(in+24),get(in+32),get(in+40)};
    if(!valid(&t))return 0;
    *m=t;return 1;
}
static int init(struct amp_session *s,uint64_t generation,uint64_t task){
    if(!s)return 0;
    *s=(struct amp_session){0};s->closed=1;
    if(!generation||!task)return 0;
    s->generation=generation;s->task=task;s->closed=0;return 1;
}
static int fault(struct amp_session *s){if(s)s->closed=1;return 0;}
static int request(struct amp_session *s,uint8_t op,uint64_t arg,uint8_t *out){
    if(!s||s->closed||s->pending||s->last==UINT64_MAX)return fault(s);
    struct amp_msg m={AMP_REQ,op,0,s->generation,s->last+1,s->task,arg,0};
    if(!encode(out,AMP_SIZE,&m))return fault(s);
    s->pending=op;s->pending_arg=arg;return 1;
}
/* AMP2 returns an authority-assigned virtual PID, not a native struct proc PID. */
static int serve(struct amp_session *s,const uint8_t *in,size_t len,uint8_t *out,uint32_t pid){
    struct amp_msg m;
    if(!s||s->closed||s->last==UINT64_MAX||!decode(&m,in,len)||m.kind!=AMP_REQ||
       m.generation!=s->generation||m.task!=s->task||m.seq!=s->last+1)return fault(s);
    m.kind=AMP_REPLY;m.result=m.op==AMP_GETPID?pid:0;
    if(!encode(out,AMP_SIZE,&m))return fault(s);
    s->last=m.seq;if(m.op==AMP_EXIT_INTENT)s->closed=1;return 1;
}
static int complete(struct amp_session *s,const uint8_t *in,size_t len,uint64_t *result){
    struct amp_msg m;
    if(!s||s->closed||!s->pending||s->last==UINT64_MAX||!result||!decode(&m,in,len)||
       m.kind!=AMP_REPLY||m.op!=s->pending||m.arg!=s->pending_arg||
       m.generation!=s->generation||m.task!=s->task||m.seq!=s->last+1)return fault(s);
    s->last=m.seq;s->pending=0;s->pending_arg=0;
    if(m.status!=AMP_OK||m.op==AMP_EXIT_INTENT)s->closed=1;
    if(m.status!=AMP_OK)return 0;
    *result=m.result;return 1;
}
