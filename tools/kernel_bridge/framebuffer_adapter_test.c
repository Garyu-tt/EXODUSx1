#include "framebuffer_adapter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    uint8_t pixels[3*20+2];memset(pixels,0xa5,sizeof pixels);
    struct exc_fb fb={pixels+1,60,20,4,3};
    struct exc_scene s;struct exc_id a,b;
    assert(!exc_init(&s,4,3));
    assert(!exc_create(&s,(struct exc_rect){-1,0,3,2},0x123456,&a));
    assert(!exc_create(&s,(struct exc_rect){1,1,2,2},0xabcdef,&b));
    assert(!exc_render(&s,0x010203,exc_fb_fill,&fb));
    const uint8_t red[]={0x12,0x34,0x56,255},top[]={0xab,0xcd,0xef,255},bg[]={1,2,3,255};
    assert(!memcmp(pixels+1,red,4));assert(!memcmp(pixels+1+20+4,top,4));
    assert(!memcmp(pixels+1+40+12,bg,4));
    for(unsigned y=0;y<3;y++)for(unsigned i=16;i<20;i++)assert(pixels[1+y*20+i]==0xa5);
    assert(pixels[0]==0xa5 && pixels[61]==0xa5);
    assert(exc_fb_fill(&fb,(struct exc_rect){3,2,2,1},0)==-1);
    fb.bytes=59;assert(exc_render(&s,0,exc_fb_fill,&fb)==-1);
    fb.bytes=60;fb.pitch=15;assert(exc_render(&s,0,exc_fb_fill,&fb)==-1);
    puts("PASS: RGBA, clipped surfaces, padded pitch, guard bytes and invalid buffers");
    return 0;
}
