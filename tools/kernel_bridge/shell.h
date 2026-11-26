#ifndef EXODUS_SHELL_H
#define EXODUS_SHELL_H
#include "supervisor.h"
#include <stdio.h>
#include <inttypes.h>
#define EXSH_MAX_LINE 128
struct exsh_app { const char *name, *program; };
struct exsh {
    struct exp_manager *pm;
    struct exs *supervisor;
    const struct exsh_app *apps;
    unsigned app_count;
    unsigned owned[EXP_MAX];
    struct exp_token tokens[EXP_MAX];
};
static inline const char *exsh_state(enum exs_state s) {
    switch(s) {
        case EXS_WAIT:return "wait"; case EXS_RUNNING:return "running";
        case EXS_READY:return "ready"; case EXS_BACKOFF:return "backoff";
        case EXS_DONE:return "done"; case EXS_FAILED:return "failed";
    }
    return "unknown";
}
/* Poll only shell-owned children; supervisor independently owns its services. */
static inline int exsh_poll(struct exsh *s,FILE *out) {
    if(!s || !s->pm || !out) return -1;
    int errors=0;
    for(unsigned i=0;i<EXP_MAX;++i) {
        if(!s->owned[i]) continue;
        struct exp_child c; int native=0;
        enum exp_status rc=exp_poll(s->pm,s->tokens[i],&c,&native);
        if(rc==EXP_PENDING) continue;
        if(rc!=EXP_OK) {
            fprintf(out,"poll slot=%u status=%d native=%d\n",i,(int)rc,native);
            errors=1; continue;
        }
        if(exp_release(s->pm,s->tokens[i])!=EXP_OK) { errors=1; continue; }
        s->owned[i]=0;
        fprintf(out,"exit identity=%" PRIu64 " code=%d signaled=%d\n",c.identity,c.code,c.signaled);
    }
    return errors || ferror(out) ? -1 : 0;
}
/* Caller supplies a complete NUL-terminated line and trusted local manifest.
   No shell expansion, implicit privilege, arbitrary paths or argument parsing. */
static inline int exsh_command(struct exsh *s,const char *line,FILE *out) {
    if(!s || !s->pm || !line || !out || (s->app_count && !s->apps)) return -1;
    size_t n=0;
    while(n<EXSH_MAX_LINE && line[n]) ++n;
    if(n==EXSH_MAX_LINE) return -1;
    while(n && (line[n-1]=='\n'||line[n-1]=='\r')) --n;
    char cmd[EXSH_MAX_LINE];
    memcpy(cmd,line,n); cmd[n]=0;
    if(!n) return 0;
    if(!strcmp(cmd,"help")) {
        fputs("help | ps | svc | status | exec NAME\n",out);
    } else if(!strcmp(cmd,"ps")) {
        fputs("managed processes (not system-wide)\n",out);
        for(unsigned i=0;i<EXP_MAX;++i) {
            const struct exp_entry *e=&s->pm->entries[i];
            if(e->occupied) fprintf(out,"slot=%u generation=%" PRIu64 " identity=%" PRIu64 " state=%s\n",
                i,e->generation,e->child.identity,e->child.exited?"exited":"tracked");
        }
    } else if(!strcmp(cmd,"svc") || !strcmp(cmd,"status")) {
        if(!s->supervisor || !s->supervisor->initialized) fputs("supervisor unavailable\n",out);
        else {
            const struct exs *v=s->supervisor;
            for(unsigned i=0;i<v->count;++i) fprintf(out,"%s state=%s restarts=%u native=%d\n",
                v->spec[i].name,exsh_state(v->services[i].state),v->services[i].restarts,v->services[i].native_error);
        }
    } else if(!strncmp(cmd,"exec ",5)) {
        const char *program=NULL;
        for(unsigned i=0;i<s->app_count;++i)
            if(s->apps[i].name && !strcmp(s->apps[i].name,cmd+5)) { program=s->apps[i].program; break; }
        if(!program) { fputs("unknown application\n",out); return -1; }
        struct exp_token t; int native=0;
        enum exp_status rc=exp_spawn(s->pm,program,&t,&native);
        if(rc!=EXP_OK) { fprintf(out,"spawn status=%d native=%d\n",(int)rc,native); return -1; }
        s->owned[t.slot]=1; s->tokens[t.slot]=t;
        fprintf(out,"spawned identity=%" PRIu64 "\n",s->pm->entries[t.slot].child.identity);
    } else {
        fputs("unknown command\n",out); return -1;
    }
    return ferror(out)?-1:0;
}
#endif
