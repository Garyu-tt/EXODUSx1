#include "compositor.h"
#include <assert.h>
#include <stdio.h>
struct canvas { uint32_t px[8][8]; unsigned draws; int fail; };
static int fill(void *ctx,struct exc_rect r,uint32_t color) {
    struct canvas *c=ctx;
    if(c->fail) return -1;
    assert(r.x>=0 && r.y>=0 && r.w>0 && r.h>0 && r.x+r.w<=8 && r.y+r.h<=8);
    ++c->draws;
    for(int32_t y=r.y;y<r.y+r.h;++y)
        for(int32_t x=r.x;x<r.x+r.w;++x) c->px[y][x]=color;
    return 0;
}
struct input { struct exc_id target; int32_t x,y; unsigned count; };
static int deliver(void *ctx,struct exc_id id,int32_t x,int32_t y) {
    struct input *i=ctx; i->target=id; i->x=x; i->y=y; ++i->count; return 0;
}
int main(void) {
    struct exc_scene s={0};
    struct exc_id a,b,c,f;
    struct canvas cv={0};
    struct input in={0};
    assert(exc_init(&s,8,8)==0);
    assert(exc_focus(&s,&f)==-1);
    assert(exc_create(&s,(struct exc_rect){-2,0,6,5},0xff0000,&a)==0);
    assert(exc_create(&s,(struct exc_rect){2,2,5,5},0x00ff00,&b)==0);
    assert(exc_create(&s,(struct exc_rect){INT32_MAX,INT32_MIN,1,1},1,&c)==0);
    assert(exc_destroy(&s,c)==0);
    assert(exc_render(&s,0,fill,&cv)==0);
    assert(cv.draws==3 && cv.px[0][0]==0xff0000 && cv.px[2][2]==0x00ff00 && cv.px[7][7]==0);
    assert(exc_press(&s,2,2,deliver,&in)==1 && in.target.slot==b.slot && in.x==0 && in.y==0);
    assert(exc_press(&s,0,0,deliver,&in)==1 && in.target.slot==a.slot && in.x==2 && in.y==0);
    assert(exc_focus(&s,&f)==0 && f.slot==a.slot);
    assert(exc_render(&s,0,fill,&cv)==0 && cv.px[2][2]==0xff0000);
    assert(exc_press(&s,7,7,deliver,&in)==0 && in.count==2);
    assert(exc_press(&s,8,0,deliver,&in)==-1);
    assert(exc_move(&s,a,(struct exc_rect){0,0,0,1})==-1);
    assert(exc_move(&s,a,(struct exc_rect){7,7,2,2})==0);
    assert(exc_render(&s,0,fill,&cv)==0 && cv.px[7][7]==0xff0000 && cv.px[2][2]==0x00ff00);
    assert(exc_destroy(&s,a)==0);
    assert(exc_create(&s,(struct exc_rect){0,0,1,1},1,&c)==0 && c.slot==a.slot && c.generation!=a.generation);
    assert(exc_raise(&s,a)==-1 && exc_destroy(&s,a)==-1);
    cv.fail=1; assert(exc_render(&s,0,fill,&cv)==-1);
    cv.fail=0;
    assert(exc_move(&s,c,(struct exc_rect){INT32_MAX,INT32_MIN,8192,8192})==0);
    assert(exc_render(&s,0,fill,&cv)==0);
    struct exc_id ids[EXC_MAX-2];
    for(unsigned i=0;i<EXC_MAX-2;++i) assert(exc_create(&s,(struct exc_rect){0,0,1,1},2,&ids[i])==0);
    assert(exc_create(&s,(struct exc_rect){0,0,1,1},2,&f)==-1);
    puts("compositor pixel, focus, clipping, capacity and stale-token assertions passed");
    return 0;
}
