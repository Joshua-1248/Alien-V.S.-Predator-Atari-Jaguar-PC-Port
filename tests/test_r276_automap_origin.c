/* R276: the Jaguar HUD map_info rectangle is HUD-local; final screen is Y+14. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Include the original renderer TU to exercise its private overlay_automap
 * with a deterministic one-pixel observation and a minimal RGB framebuffer. */
#include "../src/jaguar/avp_gpu/software_renderer.c"

/* Only the original symbols actually referenced by the retained overlay are
 * linked, courtesy of function/data sections and --gc-sections. */
s16 player_type;
u8 map_on;
AvpMapInfo map_info;
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
u32 avp_jaguar_rmw_xrgb8888(u32 dst,u16 src,const u8 *qconv,size_t n)
{ (void)dst;(void)src;(void)qconv;(void)n; return 0x00335577u; }
u32 avp_jaguar_rmw_fixed_hue_xrgb8888(u32 dst,u16 src,const u8 *qconv,size_t n)
{ return avp_jaguar_rmw_xrgb8888(dst,src,qconv,n); }
void avp_gpu_automap_sample(u32 xpos,u32 ypos,s32 sine,s32 cosine,
                             unsigned window_width,unsigned window_height,
                             int dst_x,int dst_y,int *src_x,int *src_y)
{ (void)xpos;(void)ypos;(void)sine;(void)cosine;(void)window_width;
  (void)window_height;(void)dst_x;(void)dst_y;*src_x=0;*src_y=0; }
int main(void)
{
    AvpAvpGpuState *state=(AvpAvpGpuState*)calloc(1,sizeof(*state));
    u32 *pixels=(u32*)calloc(320u*228u,sizeof(*pixels));
    AvpJaguarFramebuffer fb={320u,228u,pixels};
    const AvpMapInfo shapes[3]={{114,48,120,128},{102,52,120,120},{102,36,128,118}};
    assert(state&&pixels);
    for(unsigned species=0;species<3;species++){
        memset(pixels,0,320u*228u*sizeof(*pixels));
        map_info=shapes[species];player_type=(s16)(species*4u);map_on=1;
        state->automap_bits[0]=0x80u;
        overlay_automap(state,&fb,(const u8*)"x",1,1,0,0,0,0,0,0);
        assert(pixels[(size_t)(shapes[species].y+14)*320u+(unsigned)shapes[species].x]==0x00335577u);
        assert(pixels[(size_t)shapes[species].y*320u+(unsigned)shapes[species].x]==0u);
        assert(pixels[(size_t)(shapes[species].y+14)*320u+(unsigned)(shapes[species].x-1)]==0u);
    }
    free(pixels);free(state);
    puts("Jaguar HUD automap screen-Y origin and per-species rectangles PASS.");
    return 0;
}
