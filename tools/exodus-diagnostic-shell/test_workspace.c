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
    assert(!ws_run("r0=0; repeat 16 { m[r0]=r0; m[r0]*=r0; r0+=1; } r1=m[15]; r1/=5; r2=m[3]; r2%=4;",probe,&r));
    assert(r.r[0]==16&&r.r[1]==45&&r.r[2]==1&&r.mem[15]==225);
    assert(!ws_run("repeat 2 { repeat 3 { r0+=1; } }",probe,&r)&&r.r[0]==6);
    assert(!ws_run("r0=m[15];",probe,&r)&&r.r[0]==0);
    prior=r;
    const char *bad2[]={"repeat 0 {r0=1;}","repeat 1025 {r0=1;}","repeat 2 {r0=1;","repeat 1024 {repeat 1024 {}}","repeat 1 {repeat 1 {repeat 1 {repeat 1 {repeat 1 {r0=1;}}}}}","m[256]=1;","r0=256; m[r0]=1;","r0=m[256];","r0/=0;","r0%=0;","r0=1; }"};
    for(unsigned i=0;i<sizeof bad2/sizeof *bad2;i++){assert(ws_run(bad2[i],probe,&r));assert(!memcmp(&r,&prior,sizeof r));}
    assert(!strcmp(ws_run("repeat 1024 {repeat 1024 {}}",probe,&r),"step_limit"));
    Workspace sub={0};
    assert(!ws_put(&sub,"increment","r0+=1;"));
    assert(!ws_run_in(&sub,"repeat 3 {call increment;} assert r0 == 3; assert r0 != 4;",probe,&r));
    assert(r.r[0]==3&&r.steps==5);
    prior=r;
    assert(!ws_put(&sub,"recursive","call recursive;"));
    assert(!strcmp(ws_run_in(&sub,"call recursive;",probe,&r),"nesting"));
    assert(!strcmp(ws_run_in(&sub,"r0=1; assert r0 == 2;",probe,&r),"assertion"));
    assert(!strcmp(ws_run_in(&sub,"call absent;",probe,&r),"not_found"));
    assert(!memcmp(&r,&prior,sizeof r));
    assert(!ws_put(&sub,"increment","r0+=2;"));
    assert(!ws_run_in(&sub,"call increment;",probe,&r)&&r.r[0]==2);
    /* Ensure underlying numeric VM remains usable as well. */
    TdaVm vm;assert(!tda_vm_run("1 0 7 0",&vm)&&vm.reg[0]==7);
    puts("workspace, editing, bounded interpreter, read-only probe and transactional errors PASS");
}
