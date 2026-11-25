#define _POSIX_C_SOURCE 200809L
#include "../home/corkdeck/evcs_discovery.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
static void makefile(const char *dir,const char *name,off_t size) {
    char path[512]; assert(snprintf(path,sizeof(path),"%s/%s",dir,name)>0);
    FILE *f=fopen(path,"wb"); assert(f); assert(ftruncate(fileno(f),size)==0); assert(fclose(f)==0);
}
static void rmfile(const char *dir,const char *name) {char p[512]; assert(snprintf(p,sizeof(p),"%s/%s",dir,name)>0);assert(unlink(p)==0);}
static int emit(const char *path,enum evcs_kind k,uint64_t bytes,void *ctx) { assert(strstr(path,"Game.nsp"));assert(k==EVCS_NSP);assert(bytes==UINT64_C(5368709120));++*(int *)ctx;return 1; }
int main(void) {
    char root[]="/tmp/evcs-split.XXXXXX";assert(mkdtemp(root)); char path[512];assert(snprintf(path,sizeof(path),"%s/Game.nsp",root)>0);assert(mkdir(path,0700)==0);
    makefile(path,"00",(off_t)UINT64_C(3221225472));makefile(path,"01",(off_t)UINT64_C(2147483648));
    unsigned visits=0;struct evcs_split_info info;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_OK&&info.parts==2&&info.bytes==UINT64_C(5368709120));
    int count=0;assert(evcs_discover(root,emit,&count)==EVCS_OK&&count==1);
    visits=0;assert(evcs_split_probe(path,&visits,1,&info)==EVCS_SPLIT_LIMIT&&info.parts==0);
    rmfile(path,"01");makefile(path,"02",1); visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);count=0;assert(evcs_discover(root,emit,&count)==EVCS_OK&&count==0);rmfile(path,"02");
    makefile(path,"01",0);visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);rmfile(path,"01");
    makefile(path,"01",(off_t)UINT64_C(4294967296));visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);rmfile(path,"01");
    makefile(path,"01",1);makefile(path,"02",1);visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);rmfile(path,"02");
    makefile(path,"02.part",1);visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);rmfile(path,"02.part");rmfile(path,"01");
    char symlinkpath[512];assert(snprintf(symlinkpath,sizeof(symlinkpath),"%s/01",path)>0);assert(symlink("00",symlinkpath)==0);visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);rmfile(path,"01");
    visits=0;assert(evcs_split_probe(path,&visits,100,&info)==EVCS_SPLIT_INVALID);rmfile(path,"00");assert(rmdir(path)==0);assert(rmdir(root)==0);puts("PASS: split inventory, 5 GiB sparse, limit, gap, empty, oversize, short nonfinal, partial, symlink, singleton");
}
