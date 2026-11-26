/* Linux/POSIX reference provider; not a Horizon implementation. */
#ifndef EXODUS_PROCESS_POSIX_H
#define EXODUS_PROCESS_POSIX_H
#include "process.h"
#include <errno.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
extern char **environ;
static inline int exp_posix_spawn(void *ctx,const char *program,uint64_t *id) {
    (void)ctx;
    if(program[0]!='/') return EINVAL;
    pid_t pid;
    char *argv[]={(char *)program,NULL};
    int rc=posix_spawn(&pid,program,NULL,NULL,argv,environ);
    if(!rc) *id=(uint64_t)pid;
    return rc;
}
static inline int exp_posix_poll(void *ctx,uint64_t id,struct exp_child *out) {
    (void)ctx;
    pid_t pid=(pid_t)id;
    if(pid<=0 || (uint64_t)pid!=id) return EINVAL;
    int status=0;
    pid_t r=waitpid(pid,&status,WNOHANG);
    if(r<0) return errno;
    if(!r) return 0;
    if(WIFEXITED(status)) {
        out->exited=1; out->code=WEXITSTATUS(status); out->signaled=0;
    } else if(WIFSIGNALED(status)) {
        out->exited=1; out->code=WTERMSIG(status); out->signaled=1;
    }
    return 0;
}
#endif
