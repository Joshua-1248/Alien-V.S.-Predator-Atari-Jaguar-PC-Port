/* R330 platform-neutral Vulkan output validation. No Vulkan SDK required. */
#include "pc/pc_vulkan_readback_policy.h"
#include <string.h>
#define AVP_VK_READBACK_MAX_BYTES (64u*1024u*1024u)

int avp_vk_readback_spec(uint32_t w,uint32_t h,uint32_t format,size_t *bytes)
{
    uint64_t n=(uint64_t)w*(uint64_t)h*4u;
    if (!bytes || !w || !h || n>AVP_VK_READBACK_MAX_BYTES ||
        (format!=AVP_VK_READBACK_BGRA8_UNORM && format!=AVP_VK_READBACK_RGBA8_UNORM))
        return 0;
    *bytes=(size_t)n;
    return 1;
}

/* Diagnostic shader inputs: checkerboard source (x^y)&1, RGB encoding 2,
 * zero shade, zero bilinear weights. Each destination pixel maps to the
 * corresponding 16x16 source texel via integer modulo. */
int avp_vk_readback_check(const uint8_t *pixels,size_t bytes,uint32_t w,
                           uint32_t h,uint32_t format,uint32_t *mismatches)
{
    size_t size=0u;
    uint32_t errors=0u;
    if (!mismatches || !pixels || !avp_vk_readback_spec(w,h,format,&size) ||
        bytes<size) return 0;
    *mismatches=0u;
    for(uint32_t y=0u;y<h;y++) for(uint32_t x=0u;x<w;x++) {
        uint32_t rgb=((x^y)&1u)?0x00336699u:0x00bb8855u;
        size_t i=((size_t)y*w+x)*4u;
        uint8_t r=(uint8_t)(rgb>>16u),g=(uint8_t)(rgb>>8u),b=(uint8_t)rgb;
        uint8_t expected[4];
        if (format==AVP_VK_READBACK_BGRA8_UNORM) {
            expected[0]=b; expected[1]=g; expected[2]=r;
        } else {
            expected[0]=r; expected[1]=g; expected[2]=b;
        }
        expected[3]=255u;
        if (memcmp(pixels+i,expected,4u)!=0) errors++;
    }
    *mismatches=errors;
    return errors==0u;
}


/* Unlike the existing checkerboard parity test, this requires GPU shader
 * output to depend on the per-column wall ID from the R331 geometry buffer. */
int avp_vk_readback_check_ray_columns(const uint8_t *pixels,size_t bytes,
    uint32_t w,uint32_t h,uint32_t format,uint32_t *mismatches)
{
    size_t size=0u;
    uint32_t errors=0u;
    if (!mismatches||!pixels||!avp_vk_readback_spec(w,h,format,&size)||
        bytes<size)return 0;
    *mismatches=0u;
    for(uint32_t y=0u;y<h;y++) for(uint32_t x=0u;x<w;x++) {
        const uint32_t shift=(x/8u)&1u;
        const uint32_t rgb=((((x+shift)^y)&1u)!=0u)?0x00336699u:0x00bb8855u;
        const size_t i=((size_t)y*w+x)*4u;
        const uint8_t r=(uint8_t)(rgb>>16u),g=(uint8_t)(rgb>>8u),b=(uint8_t)rgb;
        const uint8_t expected[4]={
            format==AVP_VK_READBACK_BGRA8_UNORM?b:r,
            g,format==AVP_VK_READBACK_BGRA8_UNORM?r:b,255u
        };
        if(memcmp(pixels+i,expected,4u)!=0)errors++;
    }
    *mismatches=errors;
    return errors==0u;
}
