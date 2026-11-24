#include "nxbsd_commands.h"
#include <errno.h>
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
struct invocation_run {
    const struct nxbsd_command *cmd;
    struct nxbsd_invocation call;
};
static int invoke(struct nxbsd_context *c,void *arg)
{
    const struct invocation_run *r=arg;
    return r->cmd->fn(c,r->call.argc,r->call.argv);
}
int nxbsd_commands_pipeline(const struct nxbsd_context *parent,
                            const struct nxbsd_invocation *calls,size_t n,int *status)
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
        runs[i]=(struct invocation_run){cmd,calls[i]};
        stages[i]=(struct nxbsd_stage){invoke,&runs[i]};
    }
    return nxbsd_pipeline_run(parent,stages,n,status);
}
