/* RE39 R176: actual geometric depth, NOT luma-as-distance proxy, controls
 * enhanced Infrared SCENERY. Refine cold ambient ground and near-wall warmth
 * to the user's LEFT thermal-photo reference; protect all other vision modes. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned r(u32 c){return (c>>16)&255u;}
static unsigned g(u32 c){return (c>>8)&255u;}
static unsigned b(u32 c){return c&255u;}
int main(void)
{
    const u32 tex=0x00808080u;
    const u32 near=avp_predator_infrared_background_depth_color(tex,1.0);
    const u32 mid=avp_predator_infrared_background_depth_color(tex,3.0);
    const u32 far=avp_predator_infrared_background_depth_color(tex,5.0);
    const u32 far_bright=avp_predator_infrared_background_depth_color(0x00ffffffu,16.0);
    const u32 near_dark=avp_predator_infrared_background_depth_color(0x00000000u,1.0);
    const unsigned width=640,height=228;
    const size_t count=(size_t)width*height;
    u32 *frame=(u32*)malloc(count*sizeof(u32));
    u32 *mask=(u32*)calloc(count,sizeof(u32));
    float *wall=(float*)malloc(count*sizeof(float));
    assert(frame&&mask&&wall);
    /* Original AAF brightness adds within-band detail, but may not reverse
     * warm NEAR / cold FAR even if the far pixel is much brighter. */
    assert(r(near)>b(near) && r(near)>r(far)+100u);
    /* R176: cold ground/distant walls sit close to deep-blue reference, and
     * warm colors end by 5 cells instead of spreading to 8 as in R175. */
    assert(r(far)>=20u && r(far)<=40u);
    assert(g(far)>=8u && g(far)<=16u);
    assert(b(far)>=90u && b(far)<=115u);
    assert(r(near)>175u && g(near)<55u && b(near)<160u);
    assert(b(far)>r(far) && b(far_bright)>r(far_bright));
    assert(r(near_dark)>b(near_dark));
    assert(r(near)>r(mid) && r(mid)>r(far));
    assert(r(near)>g(near)*2u && g(far)<30u);
    assert(avp_predator_infrared_background_depth_color(tex,1.0)==near);
    assert(avp_predator_infrared_background_depth_color(tex,8.0)==far);
    assert(avp_predator_infrared_background_depth_color(tex,40.0)==far);
    assert(avp_predator_infrared_background_depth_color(tex,128.0)==far);
    /* Same pixel, same material, different true wall distances: the native
     * RGB16 sample word changes heat category regardless of texture luma. */
    for(size_t i=0;i<count;i++){frame[i]=tex;wall[i]=FLT_MAX;}
    wall[1]=1.0f; wall[3]=8.0f;
    mask[5]=1u; /* protects the entire third 2-pixel RGB16 word */
    avp_predator_infrared_background_composite_depth_rgb16(frame,mask,wall,width,height,0);
    assert(frame[0]==near && frame[1]==near);
    assert(frame[2]==far && frame[3]==far);
    assert(frame[4]==tex && frame[5]==tex);
    /* The reference's GROUND stays dark blue-violet, even close to camera;
     * unlike R175 the projected floor/ceiling ambient pass does not turn
     * almost the entire foreground pink. A nearby WALL still turns magenta. */
    assert(b(frame[200u*width+0u])>r(frame[200u*width+0u]));
    assert(r(frame[200u*width+0u])<70u);
    assert(b(frame[110u*width+0u])>r(frame[110u*width+0u]));
    assert(r(near)>r(frame[200u*width+0u])+110u);
    /* Offscreen/invalid allocations are harmless and leave input unchanged. */
    avp_predator_infrared_background_composite_depth_rgb16(NULL,mask,wall,width,height,0);
    avp_predator_infrared_background_composite_depth_rgb16(frame,NULL,wall,width,height,0);
    /* The approved independent mode bindings and actor palette persist. */
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x8880)==AVP_VISION_ENHANCED_INFRARED);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x26f8)==AVP_VISION_ORIGINAL);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x9558)==AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(avp_infrared_palette(255)==0x00fff080u);
    assert(avp_thermal_palette(255)==0x00ffecccu);
    assert(avp_thermal_heat_palette(255)==0x00ff251au);
    assert(avp_predator_background_color(AVP_VISION_ENHANCED_ELECTROMAGNETIC,0x00c7bd80u,0,0)==0x00b30ef8u);
    free(frame); free(mask); free(wall);
    puts("R176 Infrared near-wall pink, cool flat ground, distant blue-violet RGB16 PASS");
    return 0;
}
