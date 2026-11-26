#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jaguar/object_processor.h"
#include "jaguar/framebuffer.h"
#include "jaguar/avp_gpu/software_renderer.h"
#include "platform_pc_scale.h"
#include "../pause_state.h"
#include "../atomic_overlay.h"
typedef struct AvpPcVideo {AvpJaguarFramebuffer fb,world_fb; AvpAvpGpuState gpu; AvpNewPauseView pause_view; int presenting_world_hires;void *dpy;} AvpPcVideo;
static s8 hud_bright;
static unsigned redraws,presents,flushes;
static u32 expect_x=0x12345678u;
static void present_framebuffer(AvpPcVideo *v,int status,int menu){assert(!status&&menu);assert(v->presenting_world_hires);presents++;}
static void XFlush(void *d){(void)d;flushes++;}
void avp_gpu_render_maze(AvpAvpGpuState *s,AvpJaguarFramebuffer *f,const u8 *m,u16 mw,u16 mh,u32 x,u32 y,u32 angle,int ok,unsigned lev,u64 vbl,int smooth,int debug){(void)s;(void)m;(void)mw;(void)mh;(void)y;(void)angle;(void)ok;(void)lev;(void)smooth;(void)debug;assert(x==expect_x&&vbl==42);redraws++;for(size_t i=0;i<(size_t)f->width*f->height;i++)f->pixels[i]=(u32)i+(u32)hud_bright;}
#include "../pause_present.inc"
int main(void){AvpPcVideo *v=calloc(1,sizeof *v);assert(v);u8 maze=0;
    assert(avp_jaguar_framebuffer_init(&v->fb,320,228));assert(avp_jaguar_framebuffer_init(&v->world_fb,640,456));
    for(size_t i=0;i<640u*456u;i++)v->world_fb.pixels[i]=(u32)i;
    v->pause_view=(AvpNewPauseView){.maze=&maze,.x=expect_x,.vbl=42,.valid=1};v->presenting_world_hires=1;
    avp_new_pause_event(v,0x5053564fu,0,0,0);assert(v->gpu.pause_op.base_count==640u*456u);
    memset(v->world_fb.pixels,0,640u*456u*4u);avp_pc_video_present_pause(v);assert(redraws==0&&presents==1&&v->world_fb.pixels[12345]==12345);
    hud_bright=7;avp_pc_video_present_pause(v);assert(redraws==1&&v->world_fb.pixels[12345]==12352);avp_pc_video_present_pause(v);assert(redraws==1&&presents==3&&flushes==3);
    avp_new_pause_event(v,0x5052564fu,0,0,0);assert(!v->pause_view.valid&&!v->pause_view.hires&&!v->gpu.pause_op.active);
    avp_jaguar_pause_op_destroy(&v->gpu.pause_op);avp_jaguar_framebuffer_destroy(&v->world_fb);avp_jaguar_framebuffer_destroy(&v->fb);free(v);
    for(int f=0;f<2;f++)for(int s=0;s<2;s++)assert(avp_new_atomic_overlay_required(f,s)==(f||s));
    puts("PASS full-resolution pause snapshot, frozen-camera HUD redraw, restore, overlay policy");return 0;}
