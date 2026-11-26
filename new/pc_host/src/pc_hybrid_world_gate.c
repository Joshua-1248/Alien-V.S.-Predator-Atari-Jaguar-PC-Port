#include "pc/pc_hybrid_world_gate.h"
#include <limits.h>
#include <stdint.h>

int avp_hybrid_world_gate_compare(const uint8_t *expected_rgba,
    size_t expected_bytes,const uint32_t *software_world,uint32_t stride,
    uint32_t width,uint32_t height,uint32_t *mismatches)
{
    uint32_t different=0u;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!expected_rgba||!software_world||!mismatches||!width||!height||
       width>AVP_HYBRID_WORLD_GATE_MAX_PIXELS/height||stride<width||
       (size_t)width*height>SIZE_MAX/4u||
       expected_bytes!=(size_t)width*height*4u||
       (size_t)stride>SIZE_MAX/sizeof(uint32_t)/height)return 0;
    for(uint32_t y=0u;y<height;y++){
        const uint32_t *src=software_world+(size_t)y*stride;
        const uint8_t *rgb=expected_rgba+(size_t)y*width*4u;
        for(uint32_t x=0u;x<width;x++){
            const size_t i=(size_t)x*4u;
            uint32_t c=(uint32_t)rgb[i]<<16u|
                       (uint32_t)rgb[i+1u]<<8u|rgb[i+2u];
            if(rgb[i+3u]!=255u || c!=(src[x]&0xffffffu))different++;
        }
    }
    *mismatches=different;
    return different==0u;
}
