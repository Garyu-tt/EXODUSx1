#include "shell.h"
#include <assert.h>
struct mock { unsigned n; unsigned done[8]; };
static int spawn(void *ctx,const char *program,uint64_t *id) {
    struct mock *m=ctx; (void)program; *id=++m->n; return 0;
}
static int poll(void *ctx,uint64_t id,struct exp_child *out) {
    struct mock *m=ctx; out->exited=(int)m->done[id]; return 0;
}
int main(void) {
    struct mock m={0};
    struct exp_manager pm={.backend={&m,spawn,poll}};
    struct exs sup={0};
    const struct exs_spec spec[]={{"test-service","service",0,0,0,0}};
    const struct exsh_app apps[]={{"sample","sample"}};
    struct exsh s={.pm=&pm,.supervisor=&sup,.apps=apps,.app_count=1};
    FILE *f=tmpfile(); assert(f);
    assert(exs_init(&sup,&pm,spec,1)==0 && exs_tick(&sup,0)==0);
    assert(exsh_command(&s,"help\n",f)==0);
    assert(exsh_command(&s,"exec sample\r\n",f)==0 && m.n==2);
    assert(exsh_command(&s,"exec sample;anything",f)==-1 && m.n==2);
    assert(exsh_command(&s,"exec /bin/true",f)==-1 && m.n==2);
    char longline[EXSH_MAX_LINE+1]; memset(longline,'x',sizeof(longline)-1); longline[EXSH_MAX_LINE]=0;
    assert(exsh_command(&s,longline,f)==-1);
    assert(exsh_command(&s,"ps",f)==0 && exsh_command(&s,"svc",f)==0);
    m.done[1]=m.done[2]=1;
    assert(exsh_poll(&s,f)==0);
    assert(pm.entries[0].occupied && !pm.entries[1].occupied);
    assert(exs_tick(&sup,1)==0 && !pm.entries[0].occupied);
    assert(exsh_command(&s,"status",f)==0);
    rewind(f); char buf[4096]={0}; assert(fread(buf,1,sizeof(buf)-1,f)>0);
    assert(strstr(buf,"managed processes (not system-wide)"));
    assert(strstr(buf,"test-service state=done"));
    assert(strstr(buf,"exit identity=2 code=0 signaled=0"));
    assert(!strstr(buf,"exit identity=1"));
    fclose(f); puts("shell contract checks passed"); return 0;
}
