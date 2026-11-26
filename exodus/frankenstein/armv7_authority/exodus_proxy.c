/* Disposable native process lifecycle admission. No arbitrary PID control. */
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
#define SLOTS 8
#define FREE 0
#define LIVE 1
#define EXITED 2
struct slot {
    struct proc *owner; /* identity only, never dereferenced after bind */
    uint32_t pid,state,code,signal;
};
static struct slot slots[SLOTS];
static struct mtx gate;
static eventhandler_tag exit_tag;
static int stopping;
static void
proxy_exit(void *unused,struct proc *p)
{
    (void)unused;
    mtx_lock(&gate);
    for(unsigned i=0;i<SLOTS;i++)
        if(slots[i].state==LIVE && slots[i].owner==p){
            slots[i].code=p->p_xexit;
            slots[i].signal=p->p_xsig;
            slots[i].owner=NULL;
            slots[i].state=EXITED;
            break;
        }
    mtx_unlock(&gate);
}
static int
proxy_bind(SYSCTL_HANDLER_ARGS)
{
    uint32_t index,pid;
    int error;
    if(req->newptr==NULL||req->oldptr==NULL||
       req->newlen!=sizeof(index)||req->oldlen!=sizeof(pid))return EINVAL;
    error=SYSCTL_IN(req,&index,sizeof(index));
    if(error)return error;
    if(index>=SLOTS)return EINVAL;
    pid=(uint32_t)curproc->p_pid;
    mtx_lock(&gate);
    if(stopping||slots[index].state!=FREE){mtx_unlock(&gate);return EBUSY;}
    for(unsigned i=0;i<SLOTS;i++)
        if(slots[i].state==LIVE&&slots[i].owner==curproc){
            mtx_unlock(&gate);return EBUSY;
        }
    slots[index]=(struct slot){curproc,pid,LIVE,0,0};
    mtx_unlock(&gate);
    /* Failed copyout consumes the registration; exit hook still clears it. */
    return SYSCTL_OUT(req,&pid,sizeof(pid));
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_proxy_bind,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_MPSAFE,NULL,0,proxy_bind,
    "S,proxy","Self-bind current native process to disposable slot");
static int
proxy_snapshot(SYSCTL_HANDLER_ARGS)
{
    uint32_t view[SLOTS*4];
    mtx_lock(&gate);
    for(unsigned i=0;i<SLOTS;i++){
        view[4*i]=slots[i].pid;view[4*i+1]=slots[i].state;
        view[4*i+2]=slots[i].code;view[4*i+3]=slots[i].signal;
    }
    mtx_unlock(&gate);
    return SYSCTL_OUT(req,view,sizeof(view));
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_proxy_snapshot,
    CTLTYPE_OPAQUE|CTLFLAG_RD|CTLFLAG_MPSAFE,NULL,0,proxy_snapshot,
    "S,proxy","PID/state/exit/signal snapshot; never returns pointers");
static int
proxy_event(module_t mod,int event,void *arg)
{
    (void)mod;(void)arg;
    switch(event){
    case MOD_LOAD:
        if(sizeof(void*)!=4)return ENXIO;
        memset(slots,0,sizeof(slots));stopping=0;
        mtx_init(&gate,"exodus proxy",NULL,MTX_DEF);
        exit_tag=EVENTHANDLER_REGISTER(process_exit,proxy_exit,NULL,EVENTHANDLER_PRI_ANY);
        if(exit_tag==NULL){mtx_destroy(&gate);return ENOMEM;}
        return 0;
    case MOD_UNLOAD:
        mtx_lock(&gate);
        for(unsigned i=0;i<SLOTS;i++)if(slots[i].state==LIVE){
            mtx_unlock(&gate);return EBUSY;
        }
        stopping=1;mtx_unlock(&gate);
        EVENTHANDLER_DEREGISTER(process_exit,exit_tag);
        mtx_destroy(&gate);return 0;
    default:return EOPNOTSUPP;
    }
}
static moduledata_t proxy_mod={"exodus_proxy",proxy_event,NULL};
DECLARE_MODULE(exodus_proxy,proxy_mod,SI_SUB_DRIVERS,SI_ORDER_MIDDLE);
MODULE_VERSION(exodus_proxy,1);
