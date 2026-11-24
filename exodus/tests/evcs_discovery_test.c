/* cc -std=c11 -Wall -Wextra -Werror evcs_discovery_test.c -o /tmp/evcs-test && /tmp/evcs-test */
#define _POSIX_C_SOURCE 200809L
#include "../home/corkdeck/evcs_discovery.h"
#include "../home/corkdeck/evcs_pfs0.h"
#include "../home/corkdeck/evcs_graph.h"
#include "../home/corkdeck/evcs_manifest.h"
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
static void pfs0_fixture(void)
{
    unsigned char b[128]={0};
    memcpy(b,"PFS0",4);
    b[4]=2; b[8]=32; /* two records, 32 string bytes */
    b[24]=4; /* first file size */
    b[40]=4; b[48]=4; b[56]=6; /* second offset, size, name offset */
    memcpy(b+64,"a.nca",6);
    memcpy(b+70,"b.cnmt.nca",11);
    FILE *f=tmpfile();
    assert(f && fwrite(b,1,sizeof(b),f)==sizeof(b));
    struct evcs_pfs0_info info;
    assert(evcs_pfs0_probe(f,sizeof(b),&info)==EVCS_PFS0_OK);
    assert(info.entries==2 && info.nca==2 && info.meta_nca==1 && info.directory_bytes==96);
    assert(ftell(f)==96); /* no payload read */
    assert(evcs_pfs0_probe(f,100,&info)==EVCS_PFS0_INVALID); /* truncated payload */
    b[40]=2; /* overlapping ranges */
    rewind(f); assert(fwrite(b,1,sizeof(b),f)==sizeof(b));
    assert(evcs_pfs0_probe(f,sizeof(b),&info)==EVCS_PFS0_INVALID && info.entries==0);
    b[40]=4; b[56]=0; /* duplicate filename */
    rewind(f); assert(fwrite(b,1,sizeof(b),f)==sizeof(b));
    assert(evcs_pfs0_probe(f,sizeof(b),&info)==EVCS_PFS0_INVALID);
    b[56]=6; b[70]='/'; /* unsafe name */
    rewind(f); assert(fwrite(b,1,sizeof(b),f)==sizeof(b));
    assert(evcs_pfs0_probe(f,sizeof(b),&info)==EVCS_PFS0_INVALID);
    b[5]=5; /* entry count over budget */
    rewind(f); assert(fwrite(b,1,sizeof(b),f)==sizeof(b));
    assert(evcs_pfs0_probe(f,sizeof(b),&info)==EVCS_PFS0_LIMIT);
    assert(fclose(f)==0);
}
static void graph_fixture(void)
{
    struct evcs_content items[]={
        {1,1,0,EVCS_BASE}, {1,2,1,EVCS_UPDATE}, {1,2,3,EVCS_UPDATE},
        {1,3,0,EVCS_DLC}, {1,3,1,EVCS_DLC}, {1,4,0,EVCS_DLC},
        {10,11,2,EVCS_UPDATE}, {20,20,0,EVCS_BASE}
    };
    size_t n=sizeof(items)/sizeof(items[0]);
    assert(evcs_graph_validate(items,n)==EVCS_GRAPH_OK);
    struct evcs_group g=evcs_graph_group(items,n,1);
    assert(g.base==0 && g.update==2 && g.dlc_count==2);
    g=evcs_graph_group(items,n,10);
    assert(g.base==SIZE_MAX && g.update==6 && g.dlc_count==0);
    g=evcs_graph_group(items,n,20);
    assert(g.base==7 && g.update==SIZE_MAX && g.dlc_count==0);
    g=evcs_graph_group(items,n,99);
    assert(g.base==SIZE_MAX && g.update==SIZE_MAX && g.dlc_count==0);
    items[2].version=1;
    assert(evcs_graph_validate(items,n)==EVCS_GRAPH_DUPLICATE);
    items[2].version=3; items[2].application_id=10;
    assert(evcs_graph_validate(items,n)==EVCS_GRAPH_INVALID);
    items[2].application_id=1; items[2].role=EVCS_BASE;
    assert(evcs_graph_validate(items,n)==EVCS_GRAPH_INVALID);
    assert(evcs_graph_validate(NULL,EVCS_GRAPH_LIMIT+1)==EVCS_GRAPH_LIMIT_EXCEEDED);
    assert(evcs_graph_validate(NULL,1)==EVCS_GRAPH_INVALID);
    assert(evcs_graph_validate(NULL,0)==EVCS_GRAPH_OK);
}
static void manifest_fixture(void)
{
    struct evcs_content item;
    assert(evcs_manifest_parse("EVCS1 0000000000000001 0000000000000002 4294967295 update",&item));
    assert(item.application_id==1 && item.content_id==2 && item.version==UINT32_MAX && item.role==EVCS_UPDATE);
    assert(!evcs_manifest_parse("EVCS1 0000000000000001 0000000000000002 4294967296 update",&item));
    assert(!evcs_manifest_parse("EVCS1 0000000000000001 0000000000000002 1 base",&item));
    assert(!evcs_manifest_parse("EVCS1 1 2 1 update",&item));
    assert(!evcs_manifest_parse("EVCS1 0000000000000001 0000000000000002 1 update extra",&item));
    char dir[]="/tmp/evcs-manifest-XXXXXX", archive[512], path[520];
    assert(mkdtemp(dir));
    assert(snprintf(archive,sizeof(archive),"%s/game.nsp",dir)>0);
    assert(snprintf(path,sizeof(path),"%s.evcs",archive)>0);
    assert(evcs_manifest_read(archive,&item)==EVCS_MANIFEST_MISSING);
    FILE *f=fopen(path,"wb");
    assert(f && fputs("EVCS1 0000000000000001 0000000000000001 0 base\r\n",f)>=0 && !fclose(f));
    assert(evcs_manifest_read(archive,&item)==EVCS_MANIFEST_OK && item.role==EVCS_BASE);
    f=fopen(path,"ab"); assert(f && fputc(0,f)!=EOF && !fclose(f));
    assert(evcs_manifest_read(archive,&item)==EVCS_MANIFEST_INVALID);
    assert(unlink(path)==0 && symlink("/etc/passwd",path)==0);
    assert(evcs_manifest_read(archive,&item)==EVCS_MANIFEST_INVALID);
    assert(unlink(path)==0 && rmdir(dir)==0);
}
int main(void)
{
    manifest_fixture();
    pfs0_fixture();
    graph_fixture();
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
