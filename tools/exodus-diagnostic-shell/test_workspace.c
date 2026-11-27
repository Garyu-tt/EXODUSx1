#include "workspace.h"
#include <assert.h>
static int probe(const char *name,uint64_t *v){if(strcmp(name,"core_mask"))return 0;*v=15;return 1;}
int main(void){
    Workspace w={0};WsResult r={0};
    assert(!ws_put(&w,"test","r0 = core_mask(); r1 = 0o7; r0 *= r1;"));
    assert(!ws_run(ws_find(&w,"test")->source,probe,&r)&&r.r[0]==105&&r.steps==3);
    assert(!ws_put(&w,"test","r0=0b1010; r0+=0x2; r1=r0;"));
    assert(!ws_run(ws_find(&w,"test")->source,probe,&r)&&r.r[1]==12);
    WsResult prior=r;
    const char *bad[]={"r8=1;","r0=1","r0=unknown();","r0=0o8;","r0=1; r1=2; garbage","r0=1; r0+=18446744073709551616;"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){assert(ws_run(bad[i],probe,&r));assert(!memcmp(&r,&prior,sizeof r));}
    assert(ws_put(&w,"../host","r0=1;"));
    assert(ws_put(&w,"test","bad\nsource"));
    assert(!strcmp(ws_find(&w,"test")->source,"r0=0b1010; r0+=0x2; r1=r0;"));
    for(unsigned i=0;i<15;i++){char name[25];snprintf(name,sizeof name,"f%u",i);assert(!ws_put(&w,name,"r0=1;"));}
    assert(!strcmp(ws_put(&w,"extra","r0=1;"),"full"));
    assert(!ws_rm(&w,"test")&&!ws_find(&w,"test"));
    assert(!ws_put(&w,"extra","r0=1;"));
    /* Ensure underlying numeric VM remains usable as well. */
    TdaVm vm;assert(!tda_vm_run("1 0 7 0",&vm)&&vm.reg[0]==7);
    puts("workspace, editing, bounded interpreter, read-only probe and transactional errors PASS");
}
