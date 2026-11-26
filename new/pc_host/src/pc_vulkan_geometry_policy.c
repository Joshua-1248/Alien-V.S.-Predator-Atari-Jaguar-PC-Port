/* RE46 R332 — independently authored, SDK-free geometry-buffer validation. */
#include "pc/pc_vulkan_geometry_policy.h"
#include "jaguar/avp_gpu/world_scene_gpu_wire.h"
#include <math.h>
#include <string.h>
#include <limits.h>

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static double real64(const uint8_t *p)
{
    uint64_t n=(uint64_t)word(p)|((uint64_t)word(p+4)<<32);
    double v;
    memcpy(&v,&n,sizeof(v));
    return v;
}
int avp_vk_geometry_wire_valid(const uint8_t *b,size_t bytes)
{
    uint32_t width,height,hits,column_off,hit_off,first=0;
    size_t expect;
    if (!b||bytes<AVP_WORLD_GPU_WIRE_HEADER_BYTES || bytes>32u*1024u*1024u) return 0;
    if (word(b)!=AVP_WORLD_GPU_WIRE_MAGIC || word(b+4)!=AVP_WORLD_GPU_WIRE_VERSION ||
        word(b+28)!=0u) return 0;
    width=word(b+8);height=word(b+12);hits=word(b+16);
    column_off=word(b+20);hit_off=word(b+24);
    if (!width||width>AVP_WORLD_SCENE_MAX_WIDTH||
        !height||height>AVP_WORLD_SCENE_MAX_HEIGHT||
        hits>width*AVP_WORLD_SCENE_MAX_HITS ||
        column_off!=AVP_WORLD_GPU_WIRE_HEADER_BYTES ||
        hit_off!=AVP_WORLD_GPU_WIRE_HEADER_BYTES+
            width*AVP_WORLD_GPU_WIRE_COLUMN_BYTES) return 0;
    expect=(size_t)hit_off+(size_t)hits*AVP_WORLD_GPU_WIRE_HIT_BYTES;
    if (expect!=bytes) return 0;
    for (uint32_t col=0;col<width;col++) {
        const uint8_t *c=b+column_off+(size_t)col*AVP_WORLD_GPU_WIRE_COLUMN_BYTES;
        uint32_t count=word(c+4);
        double depth=real64(c+8);
        if (word(c)!=first || count>AVP_WORLD_SCENE_MAX_HITS ||
            first>hits || count>hits-first ||
            word(c+16)!=1u || word(c+20)!=0u ||
            !isfinite(depth) || depth<0.0) return 0;
        for (uint32_t i=0;i<count;i++) {
            const uint8_t *h=b+hit_off+(size_t)(first+i)*AVP_WORLD_GPU_WIRE_HIT_BYTES;
            if (!isfinite(real64(h)) || real64(h)<0.0 ||
                !isfinite(real64(h+8)) ||
                word(h+16)>255u || word(h+20)>255u ||
                word(h+24)>1u || word(h+28)>1u) return 0;
        }
        first+=count;
    }
    return first==hits;
}
int avp_vk_geometry_choose_memory(uint32_t supported,uint32_t required,
    const uint32_t *flags,uint32_t count,uint32_t *index)
{
    if (!flags || !index || count>32u) return 0;
    for(uint32_t i=0;i<count;i++)
        if ((supported&(1u<<i)) && (flags[i]&required)==required) {
            *index=i;return 1;
        }
    return 0;
}
