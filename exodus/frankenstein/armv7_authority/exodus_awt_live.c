/* Native AWT1 gate; only self-registered root test parent and own children. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/eventhandler.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include "amp_wait_contract.h"
#define N 8
static struct mtx gate;
static struct proc *parent_proc,*owners[N];
static struct ew_parent parents[N];
static struct ew_child children[N];
static eventhandler_tag tag;
static int stopping,registered;
static const uint64_t generation=UINT64_C(0x1020304050607080), parent_task=UINT64_C(0x1234567887654300);
static int bind_parent(SYSCTL_HANDLER_ARGS){
    uint32_t token,pid=(uint32_t)curproc->p_pid;
    if(!req->newptr||!req->oldptr||req->newlen!=4||req->oldlen!=4)return EINVAL;
    int e=SYSCTL_IN(req,&token,4);if(e)return e;
    if(token!=1)return EINVAL;
    mtx_lock(&gate);
    if(stopping||registered){mtx_unlock(&gate);return EBUSY;}
    registered=1;parent_proc=curproc;
    for(unsigned i=0;i<N;i++)parents[i]=(struct ew_parent){generation,parent_task+i+1,UINT64_C(0xffffffff)};
    mtx_unlock(&gate);return SYSCTL_OUT(req,&pid,4);
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_awt_parent,CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_MPSAFE,NULL,0,bind_parent,"S,awt","Self-register test parent");
static int bind_child(SYSCTL_HANDLER_ARGS){
    uint32_t i,pid=(uint32_t)curproc->p_pid;
    if(!req->newptr||!req->oldptr||req->newlen!=4||req->oldlen!=4)return EINVAL;
    int e=SYSCTL_IN(req,&i,4);if(e)return e;
    if(i>=N)return EINVAL;
    sx_slock(&proctree_lock);mtx_lock(&gate);
    if(stopping||!parent_proc||curproc->p_pptr!=parent_proc){e=EPERM;goto out;}
    for(unsigned j=0;j<N;j++)if(owners[j]==curproc){e=EBUSY;goto out;}
    if(!ew_register(&children[i],9+i,parent_task+i+1,pid)){e=EBUSY;goto out;}
    owners[i]=curproc;e=0;
out:
    mtx_unlock(&gate);sx_sunlock(&proctree_lock);
    return e?e:SYSCTL_OUT(req,&pid,4);
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_awt_child,CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_MPSAFE,NULL,0,bind_child,"S,awt","Self-register native child");
static void on_exit(void *unused,struct proc *p){
    (void)unused;mtx_lock(&gate);
    if(parent_proc==p)parent_proc=NULL;
    for(unsigned i=0;i<N;i++)if(owners[i]==p){
        uint32_t status=p->p_xsig?(uint32_t)p->p_xsig:((uint32_t)p->p_xexit&255u)<<8;
        (void)ew_exit(&children[i],status);owners[i]=NULL;break;
    }
    mtx_unlock(&gate);
}
static int exchange(SYSCTL_HANDLER_ARGS){
    uint8_t q[EW_SIZE],r[EW_SIZE];
    if(!req->newptr||!req->oldptr||req->newlen!=EW_SIZE||req->oldlen!=EW_SIZE)return EINVAL;
    int e=SYSCTL_IN(req,q,sizeof q);if(e)return e;
    struct ew_frame f;
    if(!ew_decode(&f,q,sizeof q)||f.parent<=parent_task||f.parent>parent_task+N)return EINVAL;
    unsigned i=(unsigned)(f.parent-parent_task-1);
    mtx_lock(&gate);
    if(stopping||parent_proc!=curproc)e=EPERM;
    else e=ew_serve(&parents[i],children,N,q,sizeof q,r)?0:EINVAL;
    mtx_unlock(&gate);
    return e?e:SYSCTL_OUT(req,r,sizeof r);
}
SYSCTL_PROC(_kern,OID_AUTO,exodus_awt_exchange,CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_MPSAFE,NULL,0,exchange,"S,awt","AWT1 exact-child exchange");
static int lifecycle(module_t m,int event,void *arg){
    (void)m;(void)arg;
    switch(event){
    case MOD_LOAD:
        if(sizeof(void*)!=4)return ENXIO;
        mtx_init(&gate,"exodus awt",NULL,MTX_DEF);
        tag=EVENTHANDLER_REGISTER(process_exit,on_exit,NULL,EVENTHANDLER_PRI_ANY);
        if(!tag){mtx_destroy(&gate);return ENOMEM;}return 0;
    case MOD_UNLOAD:
        mtx_lock(&gate);
        if(parent_proc){mtx_unlock(&gate);return EBUSY;}
        for(unsigned i=0;i<N;i++)if(owners[i]){mtx_unlock(&gate);return EBUSY;}
        stopping=1;mtx_unlock(&gate);
        EVENTHANDLER_DEREGISTER(process_exit,tag);mtx_destroy(&gate);return 0;
    default:return EOPNOTSUPP;
    }
}
static moduledata_t mod={"exodus_awt_live",lifecycle,NULL};
DECLARE_MODULE(exodus_awt_live,mod,SI_SUB_DRIVERS,SI_ORDER_MIDDLE);
MODULE_VERSION(exodus_awt_live,1);
