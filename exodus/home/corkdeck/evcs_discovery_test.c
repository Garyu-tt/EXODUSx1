/* cc -std=c11 -Wall -Wextra -Werror evcs_discovery_test.c -o /tmp/evcs-test && /tmp/evcs-test */
#define _POSIX_C_SOURCE 200809L
#include "evcs_discovery.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

struct tally { unsigned count[4]; };
static int collect(const char *path, enum evcs_kind kind, uint64_t bytes, void *user)
{
    struct tally *t = user;
    assert(path && bytes == 1);
    ++t->count[kind];
    return 1;
}
static int stop(const char *path, enum evcs_kind kind, uint64_t bytes, void *user)
{
    (void)path; (void)kind; (void)bytes; (void)user; return 0;
}
static void touch(const char *path, int nonempty)
{
    FILE *f = fopen(path,"wb");
    assert(f);
    if (nonempty) assert(fputc('x',f) != EOF);
    assert(fclose(f) == 0);
}
int main(void)
{
    char dir[] = "/tmp/evcs-r01-XXXXXX";
    assert(mkdtemp(dir));
    char p[512], nested[512];
    const char *names[] = {"one.NRO","two.nsp","three.XCI","four.nca","five.txt","empty.nsp","partial.nsp.part"};
    for (unsigned i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        assert(snprintf(p,sizeof(p),"%s/%s",dir,names[i]) > 0);
        touch(p,i!=5);
    }
    assert(snprintf(nested,sizeof(nested),"%s/sub",dir)>0);
    assert(mkdir(nested,0700)==0);
    assert(snprintf(p,sizeof(p),"%s/sub/child.nro",dir)>0);
    touch(p,1);
    assert(snprintf(p,sizeof(p),"%s/link.nro",dir)>0);
    assert(symlink("/etc/passwd",p)==0);
    struct tally t = {{0}};
    assert(evcs_discover(dir,collect,&t)==EVCS_OK);
    assert(t.count[EVCS_NRO]==2 && t.count[EVCS_NSP]==1 &&
           t.count[EVCS_XCI]==1 && t.count[EVCS_NCA]==1);
    assert(evcs_discover(dir,stop,NULL)==EVCS_STOP);
    assert(evcs_discover(p,collect,&t)==EVCS_IO);
    assert(unlink(p)==0);
    assert(snprintf(p,sizeof(p),"%s/sub/child.nro",dir)>0);
    assert(unlink(p)==0 && rmdir(nested)==0);
    for (unsigned i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        assert(snprintf(p,sizeof(p),"%s/%s",dir,names[i])>0);
        assert(unlink(p)==0);
    }
    assert(rmdir(dir)==0);
    assert(evcs_discover(dir,collect,&t)==EVCS_IO);
    puts("EVCS discovery smoke test passed");
    return 0;
}
