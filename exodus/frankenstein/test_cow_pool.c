#include "cow_pool.h"
#include <assert.h>
#include <stdio.h>
static uint8_t backing[4*CP_PAGE];
static struct cp_pool p;
int main(void){
    memset(backing,0xa5,sizeof backing);
    assert(!cp_init(&p,backing,sizeof backing,5));
    assert(cp_init(&p,backing,sizeof backing,4)&&cp_valid(&p));
    assert(cp_create(&p,0)&&cp_alloc(&p,0,0)&&cp_alloc(&p,0,1));
    unsigned a=p.map[0][0],b=p.map[0][1];
    for(unsigned i=0;i<CP_PAGE;i++)assert(!backing[a*CP_PAGE+i]&&!backing[b*CP_PAGE+i]);
    memset(backing+a*CP_PAGE,0x12,CP_PAGE);
    memset(backing+b*CP_PAGE,0x34,CP_PAGE);
    assert(cp_clone(&p,0,1)&&cp_clone(&p,0,2));
    assert(cp_used(&p)==2&&p.refs[a]==3&&p.refs[b]==3&&!p.copies);
    unsigned x=99,y=99;
    assert(cp_split(&p,1,0,&x)&&x!=a&&p.copies==1&&p.refs[a]==2);
    assert(!memcmp(backing+x*CP_PAGE,backing+a*CP_PAGE,CP_PAGE));
    memset(backing+x*CP_PAGE,0x56,CP_PAGE);
    assert(cp_split(&p,2,1,&y)&&y!=b&&p.copies==2&&cp_used(&p)==4);
    memset(backing+y*CP_PAGE,0x78,CP_PAGE);
    struct cp_pool before=p;uint8_t original[sizeof backing];memcpy(original,backing,sizeof backing);
    unsigned fail=99;
    assert(!cp_split(&p,0,0,&fail)&&fail==99);
    assert(!memcmp(&p,&before,sizeof p)&&!memcmp(backing,original,sizeof backing));
    assert(!cp_alloc(&p,0,2)&&!cp_clone(&p,0,0));
    assert(cp_drop(&p,1)&&cp_used(&p)==3&&p.refs[x]==0);
    assert(cp_split(&p,0,1,&fail)&&fail==b&&p.copies==2);
    assert(cp_split(&p,0,0,&fail)&&fail==x&&p.copies==3);
    assert(backing[a*CP_PAGE]==0x12&&backing[x*CP_PAGE]==0x12);
    assert(backing[b*CP_PAGE]==0x34&&backing[y*CP_PAGE]==0x78);
    assert(cp_drop(&p,2)&&cp_drop(&p,0)&&!cp_used(&p)&&cp_valid(&p));
    for(unsigned cycle=0;cycle<256;cycle++){
        assert(cp_create(&p,0)&&cp_alloc(&p,0,0));
        unsigned f=p.map[0][0];
        for(unsigned i=0;i<CP_PAGE;i++)assert(!backing[f*CP_PAGE+i]);
        memset(backing+f*CP_PAGE,0xef,CP_PAGE);
        assert(cp_clone(&p,0,1)&&cp_drop(&p,0));
        unsigned z=99;assert(cp_split(&p,1,0,&z)&&z==f&&p.copies==3);
        assert(cp_drop(&p,1)&&cp_used(&p)==0&&cp_valid(&p));
    }
    assert(p.clears==258);
    assert(!cp_drop(&p,0)&&!cp_split(&p,0,0,&fail));
    puts("PASS: bounded COW pool, zero-copy clone, OOM atomicity, 256 scrub/reuse cycles");
}
