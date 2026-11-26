#include "pc/pc_hybrid_gpu_parity.h"
#include "pc/pc_vulkan_readback_policy.h"
#include <stdint.h>
#include <stddef.h>

int avp_hybrid_gpu_reproof_for_texture_change(int gpu_already_verified,
    int texture_changed,int strict_texture_verification)
{
    if(!gpu_already_verified)return 1; /* First GPU frame still must prove. */
    return !!texture_changed && !!strict_texture_verification;
}

int avp_hybrid_gpu_parity_compare_scanlines(const uint8_t *gpu,size_t gpu_bytes,
    uint32_t width,uint32_t height,uint32_t format,
    const AvpHybridViewport *v,const AvpHybridOverlay *overlay,
    const AvpPcScanlines *scanlines,uint32_t *mismatches)
{
    size_t expected=0;
    uint32_t errors=0u;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!gpu||!mismatches||!v||!overlay||!overlay->valid||
       !overlay->world||!overlay->foreground||
       !avp_vk_readback_spec(width,height,format,&expected)||
       gpu_bytes!=expected||!v->width||!v->height||
       v->x>=width||v->y>=height||v->width>width-v->x||
       v->height>height-v->y||
       v->source_width!=overlay->width||v->source_height!=overlay->height||
       !v->source_width||!v->source_height||
       v->source_height>AVP_HYBRID_OVERLAY_MAX_PIXELS/v->source_width)return 0;
    const int scan=avp_scanline_active(scanlines);
    for(uint32_t y=0u;y<height;y++)for(uint32_t x=0u;x<width;x++){
        uint32_t rgb=0u;
        uint32_t sx,sy;
        if(avp_hybrid_viewport_source_pixel(v,x,y,&sx,&sy)){
            size_t index=(size_t)sy*overlay->width+sx;
            uint32_t fg=overlay->foreground[index];
            uint32_t alpha=fg>>24u;
            if(alpha==255u)rgb=fg&0xffffffu;
            else if(alpha==0u)rgb=overlay->world[index]&0xffffffu;
            else return 0; /* opaque differences only, never blend approx */
            if(scan)rgb=avp_scanline_darken(rgb,avp_scanline_weight(scanlines,x-v->x,y-v->y));
        }
        size_t at=((size_t)y*width+x)*4u;
        uint8_t r=(uint8_t)(rgb>>16u),g=(uint8_t)(rgb>>8u),b=(uint8_t)rgb;
        uint8_t c0=format==AVP_VK_READBACK_BGRA8_UNORM?b:r;
        uint8_t c2=format==AVP_VK_READBACK_BGRA8_UNORM?r:b;
        if(gpu[at]!=c0||gpu[at+1u]!=g||gpu[at+2u]!=c2||
           gpu[at+3u]!=255u)errors++;
    }
    *mismatches=errors;
    return errors==0u;
}

int avp_hybrid_gpu_parity_compare(const uint8_t *gpu,size_t gpu_bytes,
    uint32_t width,uint32_t height,uint32_t format,
    const AvpHybridViewport *v,const AvpHybridOverlay *overlay,uint32_t *mismatches)
{
    return avp_hybrid_gpu_parity_compare_scanlines(gpu,gpu_bytes,width,height,format,v,overlay,NULL,mismatches);
}
