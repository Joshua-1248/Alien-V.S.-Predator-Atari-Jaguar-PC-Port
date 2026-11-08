/* R41: Render real HUD-font pixels into native and split-screen-scaled
 * buffers. Fake all-ones glyph bitmap stands in for the user-owned ROM. */
#include "jaguar/avp_gpu/software_renderer.h"
#include "player.h"
#include "jaguar/avp_gpu/popup_message_color.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s16 player_type=PT_HUMAN;
static unsigned pixel(const AvpJaguarFramebuffer *fb,unsigned x,unsigned y)
{
    assert(x<fb->width&&y<fb->height);
    return fb->pixels[(size_t)y*fb->width+x];
}
int main(void)
{
    AvpAvpGpuState *gpu=calloc(1,sizeof(*gpu));
    AvpJaguarFramebuffer fb={0};
    unsigned char font[64u*19u];
    assert(gpu);
    memset(font,255,sizeof font);
    gpu->hmsg_font=font;
    gpu->hmsg_font_size=sizeof font;
    fb.width=320u;fb.height=228u;
    fb.pixels=calloc((size_t)fb.width*fb.height,sizeof(*fb.pixels));
    assert(fb.pixels);
    /* "KILLED" = 6 chars, centered in the same 192px Jaguar msg plane. */
    avp_gpu_draw_dm_kill_notice(gpu,&fb,1u,16u,PT_HUMAN);
    assert(pixel(&fb,121u,64u)==0x0000ff00u);
    /* "PLAYER 16" is 9 chars, centered at x=101 on the second line. */
    assert(pixel(&fb,101u,87u)==0x0000ff00u);
    assert(pixel(&fb,0u,0u)==0u);
    memset(fb.pixels,0,(size_t)fb.width*fb.height*sizeof(*fb.pixels));
    avp_gpu_draw_dm_kill_notice(gpu,&fb,2u,1u,PT_ALIEN);
    assert(pixel(&fb,107u,64u)==0x00b860ffu); /* "KILLED BY" */
    assert(pixel(&fb,114u,87u)==0x00b860ffu); /* "PLAYER 1" */
    memset(fb.pixels,0,(size_t)fb.width*fb.height*sizeof(*fb.pixels));
    avp_gpu_draw_dm_kill_notice(gpu,&fb,1u,17u,PT_HUMAN); /* out-of-range does nothing */
    assert(pixel(&fb,121u,64u)==0u);
    /* Explicit viewer species must win even when global player_type is P1. */
    memset(fb.pixels,0,(size_t)fb.width*fb.height*sizeof(*fb.pixels));
    avp_gpu_draw_dm_kill_notice(gpu,&fb,1u,2u,PT_PREDATOR);
    assert(pixel(&fb,127u,64u)==0x00ffff00u);
    assert(player_type==PT_HUMAN);
    free(fb.pixels);
    /* Scaled viewport: native ink is mapped exactly to presentation pixels. */
    fb.width=640u;fb.height=456u;
    fb.pixels=calloc((size_t)fb.width*fb.height,sizeof(*fb.pixels));
    assert(fb.pixels);
    avp_gpu_draw_dm_kill_notice(gpu,&fb,1u,16u,PT_HUMAN);
    assert(pixel(&fb,242u,128u)==0x0000ff00u);
    assert(pixel(&fb,243u,129u)==0x0000ff00u);
    assert(pixel(&fb,202u,174u)==0x0000ff00u);
    assert(pixel(&fb,0u,0u)==0u);
    free(fb.pixels);
    /* Only the Jaguar '#' glyph is lit: no pixels may appear.  This
     * catches a punctuation regression even if centered text moves. */
    memset(font,0,sizeof font);
    font[(26u*13u)/8u]|=(unsigned char)(0x80u>>((26u*13u)&7u));
    fb.width=320u;fb.height=228u;
    fb.pixels=calloc((size_t)fb.width*fb.height,sizeof(*fb.pixels));
    assert(fb.pixels);
    avp_gpu_draw_dm_kill_notice(gpu,&fb,1u,16u,PT_HUMAN);
    avp_gpu_draw_dm_kill_notice(gpu,&fb,2u,1u,PT_ALIEN);
    for(size_t i=0;i<(size_t)fb.width*fb.height;i++)assert(fb.pixels[i]==0u);
    /* Distinct numeric glyphs prove both roster digits still draw. */
    font[(28u*13u)/8u]|=(unsigned char)(0x80u>>((28u*13u)&7u)); /* '1' */
    font[(33u*13u)/8u]|=(unsigned char)(0x80u>>((33u*13u)&7u)); /* '6' */
    avp_gpu_draw_dm_kill_notice(gpu,&fb,1u,16u,PT_HUMAN);
    assert(pixel(&fb,192u,87u)==0x0000ff00u);
    assert(pixel(&fb,205u,87u)==0x0000ff00u);
    assert(pixel(&fb,186u,87u)==0u);
    free(fb.pixels);
    free(gpu->hud_native_tiles);
    free(gpu);
    puts("R42 species-colored HUD notice native/scaled raster: PASS");
    return 0;
}
