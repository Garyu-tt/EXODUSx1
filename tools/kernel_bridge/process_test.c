#define _POSIX_C_SOURCE 200809L
#include "process_posix.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
static void wait_child(struct exp_manager *m,struct exp_token t,int code) {
    struct exp_child child;
    int native;
    for(unsigned i=0;i<500;++i) {
        enum exp_status s=exp_poll(m,t,&child,&native);
        if(s==EXP_OK) {
            assert(child.exited && !child.signaled && child.code==code);
            return;
        }
        assert(s==EXP_PENDING || (s==EXP_NATIVE && native==EINTR));
        struct timespec delay={0,10000000};
        nanosleep(&delay,NULL);
    }
    assert(!"child timeout");
}
int main(void) {
    struct exp_manager m={0};
    struct exp_token a,b,c;
    int native;
    assert(exp_spawn(&m,"/bin/true",&a,&native)==EXP_UNSUPPORTED);
    m.backend=(struct exp_backend){NULL,exp_posix_spawn,exp_posix_poll};
    assert(exp_spawn(&m,"true",&a,&native)==EXP_NATIVE && native==EINVAL);
    assert(exp_spawn(&m,"/bin/true",&a,&native)==EXP_OK);
    assert(exp_spawn(&m,"/bin/false",&b,&native)==EXP_OK);
    assert(m.entries[a.slot].child.identity!=m.entries[b.slot].child.identity);
    assert(exp_release(&m,a)==EXP_BUSY);
    wait_child(&m,b,1);
    wait_child(&m,a,0);
    wait_child(&m,a,0);
    assert(exp_release(&m,a)==EXP_OK);
    assert(exp_release(&m,b)==EXP_OK);
    assert(exp_spawn(&m,"/bin/true",&c,&native)==EXP_OK);
    assert(c.slot==a.slot && c.generation!=a.generation);
    struct exp_child out;
    assert(exp_poll(&m,a,&out,&native)==EXP_INVALID);
    wait_child(&m,c,0);
    assert(exp_release(&m,c)==EXP_OK);
    puts("real POSIX child lifecycle smoke passed");
    return 0;
}
