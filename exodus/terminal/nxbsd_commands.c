#include "nxbsd_commands.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int write_all(struct nxbsd_context *c,int fd,const void *data,size_t n)
{
    const unsigned char *p=data;
    while(n) {
        ssize_t w=nxbsd_fd_write(&c->fd,fd,p,n,0);
        if(w<0) { if(errno==EINTR) continue; return -1; }
        if(!w) { errno=EIO; return -1; }
        p+=w; n-=(size_t)w;
    }
    return 0;
}
static int out(struct nxbsd_context *c,const char *s)
{
    return write_all(c,1,s,strlen(s));
}
static int echo_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{
    for(int i=1;i<argc;i++)
        if((i>1 && out(c," ")) || out(c,argv[i])) return 1;
    return out(c,"\n")?1:0;
}
static int cat_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{
    if(argc!=1 && !(argc==2 && !strcmp(argv[1],"-"))) return 2;
    unsigned char buf[512];
    for(;;) {
        ssize_t n=nxbsd_fd_read(&c->fd,0,buf,sizeof(buf),0);
        if(n<0) { if(errno==EINTR) continue; return 1; }
        if(!n) return 0;
        if(write_all(c,1,buf,(size_t)n)) return 1;
    }
}
static int wc_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{
    if(argc!=2 || strcmp(argv[1],"-c")) return 2;
    unsigned char buf[512];
    uint64_t total=0;
    for(;;) {
        ssize_t n=nxbsd_fd_read(&c->fd,0,buf,sizeof(buf),0);
        if(n<0) { if(errno==EINTR) continue; return 1; }
        if(!n) break;
        if(UINT64_MAX-total<(uint64_t)n) return 1;
        total+=(uint64_t)n;
    }
    char result[32];
    int n=snprintf(result,sizeof(result),"%llu\n",(unsigned long long)total);
    return n<0 || (size_t)n>=sizeof(result) || write_all(c,1,result,(size_t)n)?1:0;
}
static int pwd_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{
    (void)argv;
    return argc!=1?2:(out(c,c->cwd)||out(c,"\n")?1:0);
}
static int printenv_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{
    if(argc!=2) return 2;
    const char *v=nxbsd_context_getenv(c,argv[1]);
    return !v || out(c,v) || out(c,"\n")?1:0;
}
static int true_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{ (void)c; (void)argc; (void)argv; return 0; }
static int false_cmd(struct nxbsd_context *c,int argc,const char *const *argv)
{ (void)c; (void)argc; (void)argv; return 1; }
static const struct nxbsd_command registry[]={
    {"echo",echo_cmd},{"cat",cat_cmd},{"wc",wc_cmd},{"pwd",pwd_cmd},
    {"printenv",printenv_cmd},{"true",true_cmd},{"false",false_cmd}
};
const struct nxbsd_command *nxbsd_command_lookup(const char *name)
{
    if(!name || !*name) return NULL;
    for(size_t i=0;i<sizeof(registry)/sizeof(registry[0]);i++)
        if(!strcmp(name,registry[i].name)) return &registry[i];
    return NULL;
}
static int valid_argv(int argc,const char *const *argv)
{
    if(argc<1 || argc>NXBSD_ARG_MAX || !argv) return 0;
    for(int i=0;i<argc;i++) if(!argv[i]) return 0;
    return *argv[0]!=0;
}
int nxbsd_command_run(struct nxbsd_context *c,int argc,const char *const *argv)
{
    if(!c || !valid_argv(argc,argv)) { errno=EINVAL; return 2; }
    const struct nxbsd_command *cmd=nxbsd_command_lookup(argv[0]);
    if(!cmd) { errno=ENOENT; return 127; }
    return cmd->fn(c,argc,argv);
}
static int redirect_one(struct nxbsd_context *c,const struct nxbsd_redir *r)
{
    if(r->kind==NXBSD_REDIR_CLOSE) {
        if(nxbsd_fd_close(&c->fd,r->fd)<0 && errno!=EBADF) return -1;
        return 0;
    }
    if(r->kind==NXBSD_REDIR_DUP)
        return nxbsd_fd_dup2(&c->fd,r->source,r->fd)<0?-1:0;
    int flags;
    switch(r->kind) {
        case NXBSD_REDIR_IN: flags=O_RDONLY; break;
        case NXBSD_REDIR_OUT: flags=O_WRONLY|O_CREAT|O_TRUNC; break;
        case NXBSD_REDIR_APPEND: flags=O_WRONLY|O_CREAT|O_APPEND; break;
        case NXBSD_REDIR_EXCLUSIVE: flags=O_WRONLY|O_CREAT|O_EXCL; break;
        default: errno=EINVAL; return -1;
    }
    char path[NXBSD_CWD_MAX];
    int n;
    if(r->path[0]=='/' || !strncmp(r->path,"sdmc:/",6))
        n=snprintf(path,sizeof(path),"%s",r->path);
    else
        n=snprintf(path,sizeof(path),"%s/%s",c->cwd,r->path);
    if(n<0 || (size_t)n>=sizeof(path)) { errno=ENAMETOOLONG; return -1; }
#ifdef __SWITCH__
    /* Do not select other devoptab devices through a path operand. */
    if(strchr(!strncmp(path,"sdmc:/",6)?path+6:path,':')) { errno=EINVAL; return -1; }
#endif
    int native=open(path,flags,0666);
    if(native<0) return -1;
    int v=nxbsd_fd_adopt(&c->fd,native);
    if(v<0) { int e=errno; close(native); errno=e; return -1; }
    if(v==r->fd) return 0;
    int rc=nxbsd_fd_dup2(&c->fd,v,r->fd);
    int e=errno;
    nxbsd_fd_close(&c->fd,v);
    errno=e;
    return rc<0?-1:0;
}
struct invocation_run {
    const struct nxbsd_command *cmd;
    struct nxbsd_invocation call;
    const struct nxbsd_redir_list *redirs;
};
static int invoke(struct nxbsd_context *c,void *arg)
{
    const struct invocation_run *r=arg;
    if(r->redirs)
        for(size_t i=0;i<r->redirs->count;i++)
            if(redirect_one(c,&r->redirs->items[i])) return 1;
    return r->cmd->fn(c,r->call.argc,r->call.argv);
}
int nxbsd_commands_pipeline(const struct nxbsd_context *parent,
                            const struct nxbsd_invocation *calls,size_t n,int *status)
{
    return nxbsd_commands_pipeline_redirect(parent,calls,NULL,n,status);
}
int nxbsd_commands_pipeline_redirect(const struct nxbsd_context *parent,
                            const struct nxbsd_invocation *calls,
                            const struct nxbsd_redir_list *redirs,size_t n,int *status)
{
    if(!parent || !calls || !status || !n || n>NXBSD_PIPELINE_MAX) {
        errno=EINVAL; return -1;
    }
    struct invocation_run runs[NXBSD_PIPELINE_MAX];
    struct nxbsd_stage stages[NXBSD_PIPELINE_MAX];
    for(size_t i=0;i<n;i++) {
        if(!valid_argv(calls[i].argc,calls[i].argv)) { errno=EINVAL; return -1; }
        const struct nxbsd_command *cmd=nxbsd_command_lookup(calls[i].argv[0]);
        if(!cmd) { errno=ENOENT; return -1; }
        if(redirs) {
            if(redirs[i].count>NXBSD_REDIR_MAX) { errno=EINVAL; return -1; }
            for(size_t j=0;j<redirs[i].count;j++) {
                const struct nxbsd_redir *r=&redirs[i].items[j];
                if(r->fd<0 || r->fd>=NXBSD_FD_MAX ||
                   r->kind<NXBSD_REDIR_IN || r->kind>NXBSD_REDIR_CLOSE ||
                   (r->kind==NXBSD_REDIR_DUP && (r->source<0 || r->source>=NXBSD_FD_MAX)) ||
                   (r->kind<=NXBSD_REDIR_EXCLUSIVE && (!r->path || !*r->path))) {
                    errno=EINVAL; return -1;
                }
            }
        }
        runs[i]=(struct invocation_run){cmd,calls[i],redirs?&redirs[i]:NULL};
        stages[i]=(struct nxbsd_stage){invoke,&runs[i]};
    }
    return nxbsd_pipeline_run(parent,stages,n,status);
}
