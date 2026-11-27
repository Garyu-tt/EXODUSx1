#ifndef EXODUS_FRAMEBUFFER_ADAPTER_H
#define EXODUS_FRAMEBUFFER_ADAPTER_H
#include "compositor.h"
#include <stdint.h>
#include <stddef.h>
/* Byte-addressed RGBA8888, explicit pitch and capacity. */
struct exc_fb { uint8_t *data; size_t bytes,pitch; int32_t w,h; };
static inline int exc_fb_fill(void *opaque, struct exc_rect r, uint32_t color) {
    struct exc_fb *f=opaque;
    if(!f || !f->data || f->w<=0 || f->h<=0 || f->w>EXC_DIM || f->h>EXC_DIM ||
       f->pitch<(size_t)f->w*4 || f->pitch>f->bytes/(size_t)f->h ||
       !exc_valid_rect(r) || r.x<0 || r.y<0 || (int64_t)r.x+r.w>f->w || (int64_t)r.y+r.h>f->h) return -1;
    for(int32_t y=r.y;y<r.y+r.h;++y){
        uint8_t *p=f->data+(size_t)y*f->pitch+(size_t)r.x*4;
        for(int32_t x=0;x<r.w;++x,p+=4){
            p[0]=(uint8_t)(color>>16);p[1]=(uint8_t)(color>>8);
            p[2]=(uint8_t)color;p[3]=255;
        }
    }
    return 0;
}
#endif
