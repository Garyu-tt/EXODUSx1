#define _POSIX_C_SOURCE 200809L
#include "shell.h"
#include "process_posix.h"
#include <time.h>
/* Host demonstrator: finite approved programs only; no general-purpose shell. */
static uint64_t now_ms(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)) return UINT64_MAX;
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int pump(struct exsh *s) {
    for(unsigned i=0;i<500;++i) {
        uint64_t now=now_ms();
        if(now==UINT64_MAX || exs_tick(s->supervisor,now) || exsh_poll(s,stdout)) return -1;
        unsigned live=0;
        for(unsigned j=0;j<EXP_MAX;++j) live+=(unsigned)s->pm->entries[j].occupied;
        if(!live && s->supervisor->services[0].state==EXS_DONE) return 0;
        struct timespec delay={0,10000000}; nanosleep(&delay,NULL);
    }
    return -1;
}
int main(void) {
    struct exp_manager pm={.backend={NULL,exp_posix_spawn,exp_posix_poll}};
    struct exs supervisor={0};
    const struct exs_spec spec[]={{"host-bootstrap-test","/bin/true",0,0,0,0}};
    const struct exsh_app apps[]={{"true","/bin/true"},{"false","/bin/false"}};
    struct exsh shell={.pm=&pm,.supervisor=&supervisor,.apps=apps,.app_count=2};
    if(exs_init(&supervisor,&pm,spec,1) || pump(&shell)) return 1;
    puts("EXODUS host shell; commands: help, ps, svc, status, exec true, exec false; EOF exits.");
    char line[EXSH_MAX_LINE];
    while(fgets(line,sizeof(line),stdin)) {
        if(!strchr(line,'\n') && !feof(stdin)) {
            int c; while((c=getchar())!=EOF && c!='\n') {}
            fputs("line too long\n",stdout); continue;
        }
        (void)exsh_command(&shell,line,stdout);
        if(pump(&shell)) { fputs("child polling failed or timed out\n",stderr); return 1; }
    }
    return ferror(stdin)||ferror(stdout)?1:0;
}
