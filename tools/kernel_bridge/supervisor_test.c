#include "supervisor.h"
#include <assert.h>
#include <stdio.h>
struct mock { unsigned spawned; unsigned finished[32]; unsigned code[32]; int poll_error; };
static int spawn(void *ctx,const char *program,uint64_t *id) {
    struct mock *m=ctx; (void)program;
    assert(m->spawned<31); *id=++m->spawned; return 0;
}
static int poll(void *ctx,uint64_t id,struct exp_child *out) {
    struct mock *m=ctx;
    if(m->poll_error) return m->poll_error;
    out->exited=(int)m->finished[id]; out->code=(int)m->code[id]; return 0;
}
int main(void) {
    struct mock m={0};
    struct exp_manager pm={.backend={&m,spawn,poll}};
    struct exs s={0};
    const struct exs_spec cyclic[]={{"a","a",2,0,0,0},{"b","b",1,0,0,0}};
    assert(exs_init(&s,&pm,cyclic,2)==-1);
    const struct exs_spec spec[]={
        {"vfs","vfs",0,0,0,0},
        {"network","network",1,1,1,100},
        {"shell","shell",2,0,0,0}
    };
    assert(exs_init(&s,&pm,spec,3)==0);
    assert(exs_init(&s,&pm,spec,3)==-1);
    assert(exs_tick(&s,0)==0 && m.spawned==1);
    assert(exs_tick(&s,1)==0 && m.spawned==1);
    m.finished[1]=1;
    assert(exs_tick(&s,2)==0 && s.services[0].state==EXS_DONE && m.spawned==2);
    assert(exs_tick(&s,3)==0 && m.spawned==2);
    struct exp_token old=s.services[1].token;
    assert(exs_ready(&s,1,old)==0);
    m.poll_error=7;
    assert(exs_tick(&s,4)==0 && m.spawned==2 && s.services[1].state==EXS_RUNNING);
    m.poll_error=0;
    assert(exs_ready(&s,1,old)==0);
    assert(exs_tick(&s,5)==0 && m.spawned==3);
    m.finished[2]=1; m.code[2]=9;
    m.finished[3]=1;
    assert(exs_tick(&s,6)==0 && s.services[1].state==EXS_BACKOFF);
    assert(s.services[2].state==EXS_DONE);
    assert(exs_tick(&s,105)==0 && m.spawned==3);
    assert(exs_tick(&s,106)==0 && m.spawned==4);
    assert(exs_ready(&s,1,old)==-1);
    m.finished[4]=1;
    assert(exs_tick(&s,107)==0 && s.services[1].state==EXS_FAILED);
    assert(exs_tick(&s,108)==0 && m.spawned==4);
    assert(exs_tick(&s,107)==-1);
    for(unsigned i=0;i<EXP_MAX;++i) assert(!pm.entries[i].occupied);
    puts("supervisor lifecycle checks passed");
    return 0;
}
