/* Host integration probe: compiled against pinned generated FreeBSD nodes. */
#include "nodes.h"
#include "parser.h"
#include "expand.h"
#include "nxbsd_commands.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Validate the donor's encoded words before running any expansion.
 * Only quotes/escapes and simple named parameters are admitted.
 * No substitutions with side effects, arithmetic, globbing or tilde. */
static int name_start(unsigned char c)
{
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';
}
static int safe_word(const char *s)
{
    if(!s) return 0;
    const unsigned char *p=(const unsigned char *)s;
    while(*p) {
        unsigned char c=*p++;
        if(c==(unsigned char)CTLESC) {
            if(!*p++) return 0;
        } else if(c==(unsigned char)CTLQUOTEMARK) {
            continue;
        } else if(c==(unsigned char)CTLVAR) {
            unsigned char flags=*p++;
            if((flags & ~VSQUOTE)!=VSNORMAL || !name_start(*p)) return 0;
            do { p++; } while(name_start(*p) || (*p>='0' && *p<='9'));
            if(*p++!='=') return 0;
        } else if(c<32 || c>=127 || strchr("*?[~",c)) {
            return 0;
        }
    }
    return 1;
}
int nxbsd_ast_pipeline(const union node *n)
{
    if(!n || n->type!=NPIPE || n->npipe.backgnd) return 125;
    struct nxbsd_invocation calls[NXBSD_PIPELINE_MAX];
    const char *args[NXBSD_PIPELINE_MAX][NXBSD_ARG_MAX];
    int status[NXBSD_PIPELINE_MAX];
    size_t count=0;
    /* Complete structural preflight before expanding any stage. */
    for(const struct nodelist *p=n->npipe.cmdlist;p;p=p->next) {
        const union node *cmd=p->n;
        if(count==NXBSD_PIPELINE_MAX || !cmd || cmd->type!=NCMD ||
           cmd->ncmd.redirect || !cmd->ncmd.args ||
           cmd->ncmd.args->type!=NARG || !cmd->ncmd.args->narg.text ||
           isassignment(cmd->ncmd.args->narg.text)) return 125;
        int words=0;
        for(const union node *a=cmd->ncmd.args;a;a=a->narg.next)
            if(++words>NXBSD_ARG_MAX || a->type!=NARG ||
               a->narg.backquote || !safe_word(a->narg.text)) return 125;
        count++;
    }
    size_t i=0;
    for(const struct nodelist *p=n->npipe.cmdlist;p;p=p->next,i++) {
        struct arglist list;
        emptyarglist(&list);
        for(union node *a=p->n->ncmd.args;a;a=a->narg.next) {
            expandarg(a,&list,EXP_SPLIT);
            if(list.count>NXBSD_ARG_MAX) return 125;
        }
        if(!list.count) return 125;
        if(!nxbsd_command_lookup(list.args[0])) return 127;
        for(int j=0;j<list.count;j++) args[i][j]=list.args[j];
        calls[i]=(struct nxbsd_invocation){list.count,args[i]};
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
