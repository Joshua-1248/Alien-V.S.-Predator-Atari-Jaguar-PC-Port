/* R302: replacements must use subtexel wall coordinates, and translucent
 * retail HUD ink must be applied to every high-resolution background pixel. */
#include "jaguar/avp_gpu/hires_wall_sample.h"
#include "jaguar/avp_gpu/hud_rmw_host_composite.h"
#include "jaguar/avp_gpu/overlay_scale_composite.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint32_t add_rmw(void *unused,uint32_t src,uint16_t ink)
{
    (void)unused;
    return src+(uint32_t)ink;
}
static void check_wall(void)
{
    int a=avp_gpu_hires_wall_x(40,(40.1)/128.0,0,0,1254u);
    int b=avp_gpu_hires_wall_x(40,(40.9)/128.0,0,0,1254u);
    int refl_a=avp_gpu_hires_wall_x(87,(40.1)/128.0,1,0,1254u);
    int refl_b=avp_gpu_hires_wall_x(87,(40.9)/128.0,1,0,1254u);
    assert(a<b && b-a>=6);
    assert(refl_a>refl_b && refl_a-refl_b>=6);
    assert(avp_gpu_hires_wall_x(40,(40.1)/128.0,1,1,1254u)==a);
    assert(avp_gpu_hires_wall_x(127,1.0,0,0,1254u)==1254-1);
    assert(avp_gpu_hires_wall_y(0,400,0,1254u)==1);
    assert(avp_gpu_hires_wall_y(2,400,0,1254u)>avp_gpu_hires_wall_y(0,400,0,1254u));
    assert(avp_gpu_hires_wall_y(50,400,+127,1254u)==1253);
    assert(avp_gpu_hires_wall_y(50,400,-127,1254u)==0);
}
static void check_hud(void)
{
    enum {W=640,H=456};
    const size_t count=AVP_HUD_NATIVE_PIXELS;
    AvpHudRmwOps *ops=(AvpHudRmwOps*)calloc(count,sizeof(*ops));
    uint32_t *before=(uint32_t*)calloc(count,sizeof(uint32_t));
    uint32_t *after=(uint32_t*)calloc(count,sizeof(uint32_t));
    uint32_t *host=(uint32_t*)calloc((size_t)W*H,sizeof(uint32_t));
    uint32_t *mask=(uint32_t*)calloc((size_t)W*H,sizeof(uint32_t));
    assert(ops&&before&&after&&host&&mask);
    for(size_t i=0;i<(size_t)W*H;i++)host[i]=(uint32_t)(100u+(i&31u));
    for(size_t i=0;i<count;i++)before[i]=after[i]=100;
    /* One translucent HUD texel spans four distinct host background texels. */
    const unsigned x=31,y=42;size_t p=(size_t)y*320u+x;
    avp_gpu_hud_replay_record(ops,p,5);
    avp_gpu_hud_replay_record(ops,p,2);
    after[p]=107;
    /* Opaque status glyphs must still use the exact source artwork. */
    size_t opaque=(size_t)(y+1u)*320u+x;
    after[opaque]=0x00aaff00u;
    avp_gpu_hud_replay_scaled_composite(host,W,H,before,after,ops,
                                        add_rmw,NULL,mask,W);
    const unsigned sx=x*2u,sy=y*2u;
    assert(host[(size_t)sy*W+sx]==100u+(((size_t)sy*W+sx)&31u)+7u);
    assert(host[(size_t)sy*W+sx+1u]==100u+(((size_t)sy*W+sx+1u)&31u)+7u);
    assert(host[(size_t)sy*W+sx]!=host[(size_t)sy*W+sx+1u]);
    assert(mask[(size_t)sy*W+sx]==1u);
    assert(host[(size_t)(sy+2u)*W+sx]==0x00aaff00u);
    assert(host[0]==100u);
    /* The unchanged-Jaguar copy path is still available independently. */
    avp_gpu_overlay_scaled_composite(host,W,H,before,after);
    assert(host[(size_t)sy*W+sx]==107u);
    free(ops);free(before);free(after);free(host);free(mask);
}
int main(void)
{
    check_wall();check_hud();
    puts("R302 hires wall sampling + HUD translucent host-pixel RMW PASS");
    return 0;
}
