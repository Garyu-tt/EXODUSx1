/* QEMU display reference: fixed bounded command, no device access.
 * Uses the exact compositor/RGBA adapter also compiled into Horizon NRO. */
#include "framebuffer_adapter.h"
#include <stdio.h>
#include <string.h>
enum { WIDTH=16, HEIGHT=12, SIZE=64 };
struct input_event {struct exc_id id; int32_t x,y; unsigned delivered;};
static int deliver(void *ctx,struct exc_id id,int32_t x,int32_t y){
    struct input_event *e=ctx;
    e->id=id;e->x=x;e->y=y;e->delivered++;
    return 0;
}
int main(int argc,char **argv) {
    uint8_t q[SIZE],pixels[WIDTH*HEIGHT*4];
    if(argc!=2 || fread(q,1,sizeof q,stdin)!=sizeof q || fgetc(stdin)!=EOF) return 1;
    if(memcmp(q,"AGX1",4)||q[4]!=1||q[5]!=1||q[6]||q[7])return 2;
    for(unsigned i=15;i<SIZE;i++)if(q[i])return 2;
    unsigned x=q[8],y=q[9],w=q[10],h=q[11];
    if(!w||!h||x+w>WIDTH||y+h>HEIGHT)return 3;
    uint32_t color=((uint32_t)q[12]<<16)|((uint32_t)q[13]<<8)|q[14];
    struct exc_scene scene;struct exc_id surface;
    struct exc_fb fb={pixels,sizeof pixels,WIDTH*4,WIDTH,HEIGHT};
    if(exc_init(&scene,WIDTH,HEIGHT)||exc_create(&scene,(struct exc_rect){x,y,w,h},color,&surface)||
       exc_render(&scene,0x142030,exc_fb_fill,&fb))return 4;
    FILE *out=fopen(argv[1],"wb");if(!out)return 5;
    size_t n=fwrite(pixels,1,sizeof pixels,out);
    if(fclose(out)||n!=sizeof pixels)return 5;
    /* Deterministic reference input; no claim of physical HID. */
    struct input_event event={0};
    if(exc_press(&scene,3,5,deliver,&event)!=1||event.delivered!=1||
       event.id.slot!=surface.slot||event.id.generation!=surface.generation||
       event.x!=1||event.y!=2)return 7;
    q[5]=2;
    q[32]=1;q[33]=(uint8_t)event.x;q[34]=(uint8_t)event.y;
    memcpy(q+24,pixels+(y*WIDTH+x)*4,4);
    memcpy(q+28,pixels+(HEIGHT*WIDTH-1)*4,4);
    return fwrite(q,1,sizeof q,stdout)==sizeof q && !fflush(stdout)?0:6;
}
