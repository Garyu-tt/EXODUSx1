/* Host integration probe: compiled against pinned generated FreeBSD nodes. */
#include "nodes.h"
#include "nxbsd_commands.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Conservative literal subset. No quotes, expansion, glob, assignments,
 * redirections, background jobs or implicit fallback to host exec. */
static int literal(const char *s)
{
    if(!s || !*s) return 0;
    for(const unsigned char *p=(const unsigned char *)s;*p;p++)
        if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') ||
             (*p>='0' && *p<='9') || strchr("_./-+",*p))) return 0;
    return 1;
}
int nxbsd_ast_pipeline(const union node *n)
{
    if(!n || n->type!=NPIPE || n->npipe.backgnd) return 125;
    struct nxbsd_invocation calls[NXBSD_PIPELINE_MAX];
    const char *args[NXBSD_PIPELINE_MAX][NXBSD_ARG_MAX];
    int status[NXBSD_PIPELINE_MAX];
    size_t count=0;
    for(const struct nodelist *p=n->npipe.cmdlist;p;p=p->next) {
        const union node *cmd=p->n;
        if(count==NXBSD_PIPELINE_MAX || !cmd || cmd->type!=NCMD ||
           cmd->ncmd.redirect) return 125;
        int argc=0;
        for(const union node *a=cmd->ncmd.args;a;a=a->narg.next) {
            if(argc==NXBSD_ARG_MAX || a->type!=NARG ||
               a->narg.backquote || !literal(a->narg.text)) return 125;
            args[count][argc++]=a->narg.text;
        }
        if(!argc) return 125;
        if(!nxbsd_command_lookup(args[count][0])) return 127;
        calls[count]=(struct nxbsd_invocation){argc,args[count]};
        count++;
    }
    if(!count) return 125;
    struct nxbsd_context c;
    char cwd[NXBSD_CWD_MAX];
    if(!getcwd(cwd,sizeof(cwd)) || nxbsd_context_init(&c,cwd)) return 125;
    int rc=125;
    for(int i=0;i<3;i++) {
        int fd=dup(i);
        if(fd<0) goto done;
        int v=nxbsd_fd_adopt(&c.fd,fd);
        if(v<0) { close(fd); goto done; }
        if(v!=i) goto done;
    }
    if(nxbsd_commands_pipeline(&c,calls,count,status)==0) rc=status[count-1];
done:
    nxbsd_context_destroy(&c);
    return rc;
}
