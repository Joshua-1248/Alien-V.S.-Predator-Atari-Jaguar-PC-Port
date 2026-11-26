/* Real R160 AAF sprite loop: verifies heat mask respects wall occlusion,
 * transparency, dying actors, nonliving objects, and player proximity. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../new/jaguar/avp_gpu/software_renderer.c"
/* Source-only sprite test discards all unrelated host renderer functions via
 * -ffunction-sections/-Wl,--gc-sections. */
s16 sprite_rescale=128;
void avp_jaguar_cry16_to_rgb(u16 cry,u8 *r,u8 *g,u8 *b)
{(void)cry;*r=225;*g=192;*b=176;}
int avp_gpu_maze2_occlusion_blocks(const AvpMaze2Occlusion *o,int x,int y,float depth)
{(void)o;(void)x;(void)y;(void)depth;return 0;}
int main(void)
{
    AvpAvpGpuState *s=calloc(1,sizeof *s);
    AvpJaguarFramebuffer fb={0};
    AvpAmp a={0};
    double z[320];
    u32 *layer=calloc(320u*228u,sizeof *layer);
    unsigned int covered=0u;
    /* One frame, one source column, two nontransparent texels. */
    u8 marine[]={0,1,0,0, 0,0,0,8, 0,1, 0,4, 255,1, 0,1, 0,2};
    assert(s && layer);
    fb.width=320;fb.height=228;
    fb.pixels=calloc((size_t)fb.width*fb.height,sizeof *fb.pixels);
    assert(fb.pixels);
    s->aaf[15]=marine;s->aaf_size[15]=sizeof marine;
    a.xpos=1u<<16;a.ypos=0u;a.creature=AC_HUMAN;
    a.animseq=AS_STAND;a.animframe=0;
    for(unsigned x=0;x<320;x++)z[x]=10.0;
    draw_amp_sprite(s,&fb,&a,0,0,1,0,0,1,160,z,NULL,layer,AVP_VISION_ENHANCED_THERMAL);
    for(size_t i=0;i<(size_t)320*228;i++)if(layer[i])covered++;
    assert(covered>0); /* real visible source AAF pixels painted */
    memset(fb.pixels,0,(size_t)320*228*4);
    memset(layer,0,(size_t)320*228*4);
    a.flags=(1u<<AMP_DEAD);
    draw_amp_sprite(s,&fb,&a,0,0,1,0,0,1,160,z,NULL,layer,AVP_VISION_ENHANCED_THERMAL);
    for(size_t i=0;i<(size_t)320*228;i++)assert(layer[i]==0);
    a.flags=0;
    memset(fb.pixels,0,(size_t)320*228*4);
    for(unsigned x=0;x<320;x++)z[x]=0.1;
    draw_amp_sprite(s,&fb,&a,0,0,1,0,0,1,160,z,NULL,layer,AVP_VISION_ENHANCED_THERMAL);
    for(size_t i=0;i<(size_t)320*228;i++)assert(layer[i]==0);
    s->aaf[15]=NULL;free(layer);free(fb.pixels);free(s);
    puts("R160 real AAF sprite visibility, death, wall depth PASS");
    return 0;
}
