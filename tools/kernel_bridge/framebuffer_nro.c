/* Foreground native graphics/HID smoke; not an AMP or Wayland backend. */
#include <switch.h>
#include "framebuffer_adapter.h"
#define W 1280
#define H 720
static int clicked(void *ctx,struct exc_id id,int32_t x,int32_t y){
    (void)ctx;(void)id;(void)x;(void)y;return 0;
}
int main(void){
    Framebuffer fb;
    if(R_FAILED(framebufferCreate(&fb,nwindowGetDefault(),W,H,PIXEL_FORMAT_RGBA_8888,2)))return 1;
    framebufferMakeLinear(&fb);
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad;padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    struct exc_scene s;struct exc_id a,b,c;
    if(exc_init(&s,W,H)||exc_create(&s,(struct exc_rect){50,50,720,520},0x294766,&a)||
       exc_create(&s,(struct exc_rect){560,120,650,480},0x347b73,&b)||
       exc_create(&s,(struct exc_rect){100,580,420,80},0x9a733d,&c)){
        framebufferClose(&fb);return 2;
    }
    int32_t x=640,y=360;int touch_was_down=0;int status=0;
    while(appletMainLoop()){
        padUpdate(&pad);u64 keys=padGetButtonsDown(&pad),held=padGetButtons(&pad);
        if(keys&HidNpadButton_Plus)break;
        if(held&HidNpadButton_Left)x-=6;
        if(held&HidNpadButton_Right)x+=6;
        if(held&HidNpadButton_Up)y-=6;
        if(held&HidNpadButton_Down)y+=6;
        if(x<0)x=0;if(x>=W)x=W-1;
        if(y<0)y=0;if(y>=H)y=H-1;
        HidTouchScreenState touch={0};int down=hidGetTouchScreenStates(&touch,1)>0 && touch.count>0;
        if(down){x=(int32_t)touch.touches[0].x;y=(int32_t)touch.touches[0].y;
            if(x>=W)x=W-1;if(y>=H)y=H-1;}
        if((keys&HidNpadButton_A)||(down&&!touch_was_down))exc_press(&s,x,y,clicked,NULL);
        touch_was_down=down;
        u32 pitch=0;void *pixels=framebufferBegin(&fb,&pitch);
        if(!pixels || pitch<(u32)W*4){status=3;break;}
        struct exc_fb target={pixels,(size_t)pitch*H,pitch,W,H};
        int rc=exc_render(&s,0x141c2b,exc_fb_fill,&target);
        struct exc_rect cursor={x,y,x+10<W?10:W-x,y+10<H?10:H-y};
        if(!rc)rc=exc_fb_fill(&target,cursor,0xffffff);
        framebufferEnd(&fb);
        if(rc){status=4;break;}
    }
    framebufferClose(&fb);return status;
}
