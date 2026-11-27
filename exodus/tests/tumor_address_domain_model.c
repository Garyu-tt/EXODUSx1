/* Address-domain admission reference; not a Tegra MMU or SMMU driver.
 * Explicit host-authorized ranges and hypothetical two-stage translation.
 * No physical addresses, table writes, or assumption that APE cannot bypass.
 */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
enum { R=1,W=2,X=4, PAGE=4096 };
typedef struct {uint64_t from,to,size;unsigned perms;} range;
static int shape(range r,uint64_t from_limit,uint64_t to_limit){
    return r.size && !(r.from%PAGE) && !(r.to%PAGE) && !(r.size%PAGE) &&
           r.from<from_limit && r.to<to_limit &&
           r.size<=from_limit-r.from && r.size<=to_limit-r.to &&
           r.perms && !(r.perms&~7u) && !((r.perms&W)&&(r.perms&X));
}
static int contains(uint64_t base,uint64_t size,uint64_t a,uint64_t n){
    return n && a>=base && a-base<size && n<=size-(a-base);
}
static int translate(range r,uint64_t a,uint64_t n,unsigned access,uint64_t *out){
    if(!access || (access&~r.perms) || !contains(r.from,r.size,a,n))return 0;
    *out=r.to+(a-r.from);return 1;
}
typedef struct {range cpu,bus,allowed;unsigned mmu_proven,smmu_proven,no_bypass;} policy;
static int resolve(policy p,uint64_t va,uint64_t n,unsigned access,uint64_t *pa){
    uint64_t da,result,identity;
    const uint64_t limit32=UINT64_C(1)<<32,limit34=UINT64_C(1)<<34;
    if(!p.mmu_proven||!p.smmu_proven||!p.no_bypass)return 0;
    if(!shape(p.cpu,limit32,limit32)||!shape(p.bus,limit32,limit34)||
       !shape(p.allowed,limit34,limit34)||p.allowed.from!=p.allowed.to)return 0;
    if(!translate(p.cpu,va,n,access,&da)||
       !translate(p.bus,da,n,access,&result)||
       !translate(p.allowed,result,n,access,&identity))return 0;
    *pa=identity;return 1;
}
int main(void){
    policy p={.cpu={0x10000,0x20000,2*PAGE,R|W},
        .bus={0x20000,UINT64_C(0x100030000),2*PAGE,R|W},
        .allowed={UINT64_C(0x100030000),UINT64_C(0x100030000),2*PAGE,R|W},
        .mmu_proven=1,.smmu_proven=1,.no_bypass=1};
    uint64_t pa=0;
    assert(resolve(p,0x10100,24,R|W,&pa)&&pa==UINT64_C(0x100030100));
    assert(resolve(p,0x11fff,1,R,&pa)&&pa==UINT64_C(0x100031fff));
    assert(!resolve(p,0x11fff,2,R,&pa));
    assert(!resolve(p,0x10000,0,R,&pa));
    assert(!resolve(p,0x10000,UINT64_MAX,R,&pa));
    assert(!resolve(p,0x10000,1,X,&pa));
    assert(!resolve(p,0x10000,1,0,&pa));
    puts("PASS: explicit address domains, bounds and permissions");
    policy q=p;q.bus.to+=PAGE;
    assert(!resolve(q,0x11000,1,R,&pa));
    q=p;q.bus.perms=R;assert(!resolve(q,0x10000,1,W,&pa));
    q=p;q.cpu.perms=R|W|X;assert(!resolve(q,0x10000,1,R,&pa));
    q=p;q.cpu.from=UINT64_C(0xfffff000);assert(!resolve(q,q.cpu.from,1,R,&pa));
    q=p;q.bus.to=UINT64_C(0x3fffff000);assert(!resolve(q,0x10000,1,R,&pa));
    puts("PASS: denied out-of-carveout, permission mismatch, W+X and overflow");
    for(unsigned mask=0;mask<8;mask++){
        q=p;q.mmu_proven=mask&1;q.smmu_proven=mask&2;q.no_bypass=mask&4;
        pa=99;
        assert(resolve(q,0x10000,1,R,&pa)==(mask==7));
        if(mask!=7)assert(pa==99);
    }
    puts("PASS: all eight translation evidence combinations checked");
    return 0;
}
