#include "amp_wait_contract.h"
#include <assert.h>
#include <stdio.h>
static struct ew_frame ask(struct ew_parent *p,uint64_t child){
    return (struct ew_frame){.generation=p->generation,.seq=p->last+1,.parent=p->task,
        .child=child,.kind=EW_REQUEST,.options=EW_NOHANG};
}
static struct ew_frame call(struct ew_parent *p,struct ew_child *c,size_t n,uint64_t child){
    uint8_t q[EW_SIZE],r[EW_SIZE];struct ew_frame f=ask(p,child),out;
    assert(ew_encode(q,&f)&&ew_serve(p,c,n,q,sizeof(q),r)&&ew_match(&f,r,sizeof(r),&out));
    return out;
}
int main(void){
    const uint64_t gen=UINT64_C(0x1020304050607080), parent=UINT64_C(0x100000001);
    struct ew_parent p={gen,parent,UINT64_C(0xffffffff)},other={gen,parent+1,0};
    struct ew_child c[3]={{0}};
    assert(ew_register(c,9,parent,2001)&&ew_register(c+1,10,parent,2002));
    assert(ew_register(c+2,11,parent+1,2003));
    assert(!ew_register(c,12,parent,2004));
    struct ew_frame r=call(&p,c,3,9);
    assert(!r.pid&&!r.error&&!r.wait_status&&p.last==UINT64_C(0x100000000));
    assert(!ew_exit(c,0x7f)&&!ew_exit(c,0xffff)&&!ew_exit(c,0x10000));
    assert(ew_exit(c,23u<<8)&&!ew_exit(c,0));
    r=call(&other,c,3,9);assert(r.error==EW_ECHILD&&c[0].state==EW_ZOMBIE);
    r=call(&p,c,3,11);assert(r.error==EW_ECHILD&&c[2].state==EW_LIVE);
    uint8_t q[EW_SIZE],a[EW_SIZE],bad[EW_SIZE];struct ew_frame f=ask(&p,9);
    assert(ew_encode(q,&f));
    for(size_t n=0;n<EW_SIZE;n++)assert(!ew_serve(&p,c,3,q,n,a));
    const uint64_t before=p.last;
    memcpy(bad,q,sizeof q);bad[8]^=1;assert(!ew_serve(&p,c,3,bad,sizeof bad,a));
    memcpy(bad,q,sizeof q);bad[16]^=1;assert(!ew_serve(&p,c,3,bad,sizeof bad,a));
    memcpy(bad,q,sizeof q);bad[24]^=1;assert(!ew_serve(&p,c,3,bad,sizeof bad,a));
    memcpy(bad,q,sizeof q);bad[48]=1;assert(!ew_serve(&p,c,3,bad,sizeof bad,a));
    assert(!ew_serve(&p,c,3,q,sizeof q,NULL));
    assert(p.last==before&&c[0].state==EW_ZOMBIE);
    assert(ew_serve(&p,c,3,q,sizeof q,a)&&ew_match(&f,a,sizeof a,&r));
    assert(r.pid==2001&&r.wait_status==(23u<<8)&&!r.error&&c[0].state==EW_REAPED);
    assert(!ew_serve(&p,c,3,q,sizeof q,a));
    for(unsigned i=0;i<4;i++){
        memcpy(bad,a,sizeof a);bad[8+8*i]^=1;assert(!ew_match(&f,bad,sizeof bad,&r));
    }
    r=call(&p,c,3,9);assert(r.error==EW_ECHILD);
    assert(ew_exit(c+1,15));r=call(&p,c,3,10);
    assert(r.pid==2002&&r.wait_status==15&&!r.error);
    r=call(&p,c,3,1234);assert(r.error==EW_ECHILD);
    p.last=UINT64_MAX;assert(!ew_serve(&p,c,3,q,sizeof q,a));
    printf("PASS: remote exact-child WNOHANG, 64 truncated frames, ownership, native wait encoding, replay and one-shot reap (width=%zu)\n",sizeof(void*)*8);
}
