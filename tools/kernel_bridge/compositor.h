#ifndef EXODUS_COMPOSITOR_H
#define EXODUS_COMPOSITOR_H
#include <stdint.h>
#include <stddef.h>
#define EXC_MAX 16
#define EXC_DIM 8192
struct exc_rect { int32_t x,y,w,h; };
struct exc_id { unsigned slot; uint64_t generation; };
struct exc_surface { uint64_t generation; int used; struct exc_rect rect; uint32_t color; };
struct exc_scene {
    int32_t width,height;
    unsigned count, order[EXC_MAX];
    struct exc_surface surfaces[EXC_MAX];
};
typedef int (*exc_fill_fn)(void *,struct exc_rect,uint32_t);
typedef int (*exc_input_fn)(void *,struct exc_id,int32_t,int32_t);
static inline int exc_valid_rect(struct exc_rect r) {
    return r.w>0 && r.h>0 && r.w<=EXC_DIM && r.h<=EXC_DIM;
}
static inline int exc_init(struct exc_scene *s,int32_t w,int32_t h) {
    if(!s || w<=0 || h<=0 || w>EXC_DIM || h>EXC_DIM) return -1;
    *s=(struct exc_scene){.width=w,.height=h};
    return 0;
}
static inline struct exc_surface *exc_get(struct exc_scene *s,struct exc_id id) {
    if(!s || id.slot>=EXC_MAX) return NULL;
    struct exc_surface *p=&s->surfaces[id.slot];
    return p->used && p->generation==id.generation ? p:NULL;
}
static inline int exc_create(struct exc_scene *s,struct exc_rect r,uint32_t color,struct exc_id *id) {
    if(!s || !id || !exc_valid_rect(r) || s->width<=0 || s->height<=0 || s->count>=EXC_MAX) return -1;
    unsigned i;
    for(i=0;i<EXC_MAX;++i)
        if(!s->surfaces[i].used && s->surfaces[i].generation!=UINT64_MAX) break;
    if(i==EXC_MAX) return -1;
    struct exc_surface *p=&s->surfaces[i];
    ++p->generation; p->used=1; p->rect=r; p->color=color;
    s->order[s->count++]=i;
    *id=(struct exc_id){i,p->generation};
    return 0;
}
static inline int exc_move(struct exc_scene *s,struct exc_id id,struct exc_rect r) {
    struct exc_surface *p=exc_get(s,id);
    if(!p || !exc_valid_rect(r)) return -1;
    p->rect=r; return 0;
}
static inline int exc_raise(struct exc_scene *s,struct exc_id id) {
    if(!exc_get(s,id)) return -1;
    unsigned i=0;
    while(i<s->count && s->order[i]!=id.slot) ++i;
    if(i==s->count) return -1;
    for(;i+1<s->count;++i) s->order[i]=s->order[i+1];
    s->order[s->count-1]=id.slot;
    return 0;
}
static inline int exc_destroy(struct exc_scene *s,struct exc_id id) {
    struct exc_surface *p=exc_get(s,id);
    if(!p) return -1;
    unsigned i=0;
    while(i<s->count && s->order[i]!=id.slot) ++i;
    if(i==s->count) return -1;
    for(;i+1<s->count;++i) s->order[i]=s->order[i+1];
    --s->count; p->used=0;
    return 0;
}
static inline int exc_focus(const struct exc_scene *s,struct exc_id *id) {
    if(!s || !id || !s->count) return -1;
    unsigned i=s->order[s->count-1];
    *id=(struct exc_id){i,s->surfaces[i].generation};
    return 0;
}
static inline int exc_clip(const struct exc_scene *s,struct exc_rect r,struct exc_rect *out) {
    int64_t x0=r.x,y0=r.y,x1=(int64_t)r.x+r.w,y1=(int64_t)r.y+r.h;
    if(x0<0) x0=0;
    if(y0<0) y0=0;
    if(x1>s->width) x1=s->width;
    if(y1>s->height) y1=s->height;
    if(x1<=x0 || y1<=y0) return 0;
    *out=(struct exc_rect){(int32_t)x0,(int32_t)y0,(int32_t)(x1-x0),(int32_t)(y1-y0)};
    return 1;
}
/* Painter order, opaque RGB colors; callback owns rendering, not scene state. */
static inline int exc_render(const struct exc_scene *s,uint32_t bg,exc_fill_fn fill,void *ctx) {
    if(!s || !fill || s->width<=0 || s->height<=0) return -1;
    if(fill(ctx,(struct exc_rect){0,0,s->width,s->height},bg)) return -1;
    for(unsigned i=0;i<s->count;++i) {
        const struct exc_surface *p=&s->surfaces[s->order[i]];
        struct exc_rect clip;
        if(exc_clip(s,p->rect,&clip) && fill(ctx,clip,p->color)) return -1;
    }
    return 0;
}
/* Pointer press picks topmost visible surface and raises it. No capture yet. */
static inline int exc_press(struct exc_scene *s,int32_t x,int32_t y,exc_input_fn deliver,void *ctx) {
    if(!s || !deliver || x<0 || y<0 || x>=s->width || y>=s->height) return -1;
    for(unsigned i=s->count;i>0;--i) {
        unsigned slot=s->order[i-1];
        struct exc_surface *p=&s->surfaces[slot];
        int64_t lx=(int64_t)x-p->rect.x,ly=(int64_t)y-p->rect.y;
        if(lx<0 || ly<0 || lx>=p->rect.w || ly>=p->rect.h) continue;
        struct exc_id id={slot,p->generation};
        if(exc_raise(s,id)) return -1;
        return deliver(ctx,id,(int32_t)lx,(int32_t)ly)?-1:1;
    }
    return 0;
}
#endif
