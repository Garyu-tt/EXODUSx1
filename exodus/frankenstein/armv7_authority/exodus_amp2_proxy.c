/* Native FreeBSD kernel AMP2 admission, not remote process semantics. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/eventhandler.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include "amp2_pid_contract.c"
#define LANES 8
static struct amp_session lanes[LANES];
static struct mtx gate;
static unsigned transactions, exited;
static struct proc *owner[LANES];
static uint8_t actual_code[LANES], actual_signal[LANES], observed[LANES];
static eventhandler_tag exit_tag;
static int stopping;
static uint32_t vpid[LANES];
static uint8_t exit_status[LANES];
static const uint64_t generation=UINT64_C(0x1020304050607080);
static const uint64_t base=UINT64_C(0x1234567887654300);

/* Bind only the calling process; never accept an external PID or pointer. */
static int
proxy_bind(SYSCTL_HANDLER_ARGS)
{
    uint32_t i,pid=(uint32_t)curproc->p_pid;
    if(req->newptr==NULL||req->oldptr==NULL||req->newlen!=4||req->oldlen!=4)return EINVAL;
    int error=SYSCTL_IN(req,&i,sizeof(i));
    if(error)return error;
    if(i>=LANES||!pid)return EINVAL;
    mtx_lock(&gate);
    if(stopping||vpid[i]){mtx_unlock(&gate);return EBUSY;}
    for(unsigned j=0;j<LANES;j++)if(owner[j]==curproc){mtx_unlock(&gate);return EBUSY;}
    owner[i]=curproc;vpid[i]=pid;
    mtx_unlock(&gate);
    return SYSCTL_OUT(req,&pid,sizeof(pid));
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_amp2_bind,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_MPSAFE,NULL,0,proxy_bind,
    "S,proxy","Bind current native process");
static void
proxy_exit(void *arg,struct proc *p)
{
    (void)arg;
    mtx_lock(&gate);
    for(unsigned i=0;i<LANES;i++)if(owner[i]==p){
        actual_code[i]=p->p_xexit;actual_signal[i]=p->p_xsig;
        observed[i]=1;owner[i]=NULL;break;
    }
    mtx_unlock(&gate);
}
static int
proxy_snapshot(SYSCTL_HANDLER_ARGS)
{
    uint32_t out[LANES*4];
    mtx_lock(&gate);
    for(unsigned i=0;i<LANES;i++){
        out[4*i]=vpid[i];out[4*i+1]=observed[i];
        out[4*i+2]=actual_code[i];out[4*i+3]=actual_signal[i];
    }
    mtx_unlock(&gate);
    return SYSCTL_OUT(req,out,sizeof(out));
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_amp2_snapshot,
    CTLTYPE_OPAQUE|CTLFLAG_RD|CTLFLAG_MPSAFE,NULL,0,proxy_snapshot,
    "S,proxy","Native exit evidence");
static int
amp_exchange(SYSCTL_HANDLER_ARGS)
{
    uint8_t q[AMP_SIZE],r[AMP_SIZE];
    struct amp_msg m;
    int error;
    if (req->newptr==NULL || req->oldptr==NULL ||
        req->newlen!=AMP_SIZE || req->oldlen!=AMP_SIZE)
        return (EINVAL);
    error=SYSCTL_IN(req,q,sizeof(q));
    if (error!=0) return (error);
    if (!decode(&m,q,sizeof(q)) || m.kind!=AMP_REQ ||
        m.task<=base || m.task>base+LANES)
        return (EINVAL);
    mtx_lock(&gate);
    unsigned i=(unsigned)(m.task-base-1);
    uint32_t pid=vpid[i];
    if (stopping || !pid || !owner[i] || observed[i] ||
        !serve(&lanes[i],q,sizeof(q),r,pid)) {
        mtx_unlock(&gate);
        return (EINVAL);
    }
    if (m.op==AMP_EXIT_INTENT) {
        exit_status[i]=(uint8_t)m.arg;
        exited++;
    }
    transactions++;
    mtx_unlock(&gate);
    /* A copyout failure consumes this sequence; never replay a side effect. */
    return (SYSCTL_OUT(req,r,sizeof(r)));
}
SYSCTL_PROC(_kern, OID_AUTO, exodus_amp2,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_MPSAFE,
    NULL, 0, amp_exchange, "S,amp2", "EXODUS disposable AMP2 fixture");

static int
amp_caller_pid(SYSCTL_HANDLER_ARGS)
{
    int pid=curproc->p_pid;
    return (SYSCTL_OUT(req,&pid,sizeof(pid)));
}
SYSCTL_PROC(_kern, OID_AUTO, exodus_amp2_caller_pid,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, amp_caller_pid, "I", "Native caller PID admission diagnostic");

static int
amp_count(SYSCTL_HANDLER_ARGS)
{
    unsigned n;
    mtx_lock(&gate);
    n=transactions;
    mtx_unlock(&gate);
    return (SYSCTL_OUT(req,&n,sizeof(n)));
}
SYSCTL_PROC(_kern, OID_AUTO, exodus_amp2_count,
    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, amp_count, "IU", "Completed kernel AMP2 transactions");

static int
amp_audit(SYSCTL_HANDLER_ARGS)
{
    unsigned n=0;
    mtx_lock(&gate);
    for(unsigned i=0;i<LANES;i++)
        if(vpid[i] && lanes[i].closed && exit_status[i]==23 &&
           observed[i] && actual_code[i]==23 && actual_signal[i]==0)n++;
    if(exited!=n)n=0;
    mtx_unlock(&gate);
    return SYSCTL_OUT(req,&n,sizeof(n));
}
SYSCTL_PROC(_kern, OID_AUTO, exodus_amp2_exited23,
    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, amp_audit, "IU", "Virtual tasks closed with status 23");
static int
amp_event(module_t mod,int event,void *arg)
{
    (void)mod;(void)arg;(void)request;(void)complete;
    switch(event) {
    case MOD_LOAD:
        if(sizeof(void *)!=4)return (ENXIO);
        mtx_init(&gate,"exodus amp2",NULL,MTX_DEF);
        transactions=exited=0;stopping=0;
        memset(owner,0,sizeof(owner));memset(observed,0,sizeof(observed));
        memset(actual_code,0,sizeof(actual_code));memset(actual_signal,0,sizeof(actual_signal));
        memset(vpid,0,sizeof(vpid));memset(exit_status,0,sizeof(exit_status));
        for(unsigned i=0;i<LANES;i++){
            if(!init(&lanes[i],generation,base+i+1)){
                mtx_destroy(&gate);return (EINVAL);
            }
            lanes[i].last=UINT64_C(0xffffffff);
        }
        exit_tag=EVENTHANDLER_REGISTER(process_exit,proxy_exit,NULL,EVENTHANDLER_PRI_ANY);
        if(exit_tag==NULL){mtx_destroy(&gate);return ENOMEM;}
        printf("EXODUS_ARMV7_KERNEL_AMP2_PROXY_LOADED\n");
        return (0);
    case MOD_UNLOAD:
        mtx_lock(&gate);
        for(unsigned i=0;i<LANES;i++)if(owner[i]){mtx_unlock(&gate);return EBUSY;}
        stopping=1;mtx_unlock(&gate);
        EVENTHANDLER_DEREGISTER(process_exit,exit_tag);
        mtx_destroy(&gate);
        return (0);
    default:return (EOPNOTSUPP);
    }
}
static moduledata_t amp_mod={"exodus_amp2_proxy",amp_event,NULL};
DECLARE_MODULE(exodus_amp2_proxy,amp_mod,SI_SUB_DRIVERS,SI_ORDER_MIDDLE);
MODULE_VERSION(exodus_amp2_proxy,1);
