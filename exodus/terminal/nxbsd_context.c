#include "nxbsd_context.h"
#include <errno.h>
#include <string.h>
static int bad(int e) { errno=e; return -1; }
static int valid_key(const char *s)
{
    return s && *s && !strchr(s,'=') && strlen(s)<NXBSD_ENV_KEY;
}
int nxbsd_context_setcwd(struct nxbsd_context *c, const char *cwd)
{
    if (!c || !cwd || !*cwd) return bad(EINVAL);
    if (cwd[0]!='/' && strncmp(cwd,"sdmc:/",6)) return bad(EINVAL);
    size_t n=strlen(cwd);
    if (n>=sizeof(c->cwd)) return bad(ENAMETOOLONG);
    memmove(c->cwd,cwd,n+1);
    return 0;
}
int nxbsd_context_init(struct nxbsd_context *c, const char *cwd)
{
    if (!c) return bad(EINVAL);
    memset(c,0,sizeof(*c));
    nxbsd_fd_init(&c->fd);
    return nxbsd_context_setcwd(c,cwd);
}
int nxbsd_context_clone(struct nxbsd_context *dst, const struct nxbsd_context *src)
{
    if (!dst || !src || dst==src || !src->cwd[0]) return bad(EINVAL);
    memset(dst,0,sizeof(*dst));
    nxbsd_fd_init(&dst->fd);
    if (nxbsd_fd_clone(&dst->fd,&src->fd)) return -1;
    memcpy(dst->env,src->env,sizeof(dst->env));
    memcpy(dst->cwd,src->cwd,sizeof(dst->cwd));
    dst->status=src->status;
    return 0;
}
void nxbsd_context_destroy(struct nxbsd_context *c)
{
    if (!c) return;
    nxbsd_fd_clear(&c->fd);
    memset(c,0,sizeof(*c));
}
static int find(const struct nxbsd_context *c, const char *key)
{
    for (int i=0;i<NXBSD_ENV_MAX;i++)
        if (c->env[i].key[0] && !strcmp(c->env[i].key,key)) return i;
    return -1;
}
int nxbsd_context_setenv(struct nxbsd_context *c,const char *key,const char *value)
{
    if (!c || !valid_key(key) || !value) return bad(EINVAL);
    if (strlen(value)>=NXBSD_ENV_VALUE) return bad(E2BIG);
    int i=find(c,key);
    if (i<0)
        for (int j=0;j<NXBSD_ENV_MAX;j++)
            if (!c->env[j].key[0]) { i=j; break; }
    if (i<0) return bad(ENOSPC);
    memmove(c->env[i].value,value,strlen(value)+1);
    memmove(c->env[i].key,key,strlen(key)+1);
    return 0;
}
const char *nxbsd_context_getenv(const struct nxbsd_context *c,const char *key)
{
    if (!c || !valid_key(key)) { errno=EINVAL; return NULL; }
    int i=find(c,key);
    return i<0 ? NULL : c->env[i].value;
}
int nxbsd_context_unsetenv(struct nxbsd_context *c,const char *key)
{
    if (!c || !valid_key(key)) return bad(EINVAL);
    int i=find(c,key);
    if (i>=0) memset(&c->env[i],0,sizeof(c->env[i]));
    return 0;
}
