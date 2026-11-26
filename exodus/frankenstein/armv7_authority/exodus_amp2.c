/* Native FreeBSD kernel AMP2 admission, not remote process semantics. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include "amp2_pid_contract.c"
#define LANES 8
static struct amp_session lanes[LANES];
static struct mtx gate;
static unsigned transactions, exited, next_pid;
static uint32_t vpid[LANES];
static uint8_t exit_status[LANES];
static const uint64_t generation=UINT64_C(0x1020304050607080);
static const uint64_t base=UINT64_C(0x1234567887654300);

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
    /* First valid GETPID creates authority-owned metadata, not a struct proc.
     * Allocation is committed only after frame/session validation succeeds. */
    uint32_t pid=vpid[i];
    if (m.op==AMP_GETPID && !pid) pid=next_pid;
    if (!pid || !serve(&lanes[i],q,sizeof(q),r,pid)) {
        mtx_unlock(&gate);
        return (EINVAL);
    }
    if (m.op==AMP_GETPID && !vpid[i]) {
        vpid[i]=pid;
        next_pid++;
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
    NULL, 0, amp_exchange, "S,amp1", "EXODUS disposable AMP2 fixture");

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
        if(vpid[i] && lanes[i].closed && exit_status[i]==23)n++;
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
        mtx_init(&gate,"exodus amp1",NULL,MTX_DEF);
        transactions=exited=0;next_pid=21001;
        memset(vpid,0,sizeof(vpid));memset(exit_status,0,sizeof(exit_status));
        for(unsigned i=0;i<LANES;i++){
            if(!init(&lanes[i],generation,base+i+1)){
                mtx_destroy(&gate);return (EINVAL);
            }
            lanes[i].last=UINT64_C(0xffffffff);
        }
        printf("EXODUS_ARMV7_KERNEL_AMP_LOADED\n");
        return (0);
    case MOD_UNLOAD:
        mtx_destroy(&gate);
        return (0);
    default:return (EOPNOTSUPP);
    }
}
static moduledata_t amp_mod={"exodus_amp2",amp_event,NULL};
DECLARE_MODULE(exodus_amp2,amp_mod,SI_SUB_DRIVERS,SI_ORDER_MIDDLE);
MODULE_VERSION(exodus_amp2,1);
