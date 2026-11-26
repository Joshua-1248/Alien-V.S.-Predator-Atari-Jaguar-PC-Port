/* R335: independently authored bounded multi-column ray diagnostic. */
#include "pc/pc_vulkan_ray_columns.h"
#include "jaguar/avp_gpu/world_scene_gpu_wire.h"
#include <string.h>

static void le32(uint8_t *b,uint32_t value)
{
    for(unsigned i=0;i<4u;i++)b[i]=(uint8_t)(value>>(i*8u));
}
static void le_double(uint8_t *b,double value)
{
    uint64_t bits;
    memcpy(&bits,&value,sizeof(bits));
    le32(b,(uint32_t)bits);
    le32(b+4u,(uint32_t)(bits>>32u));
}
size_t avp_vk_diag_ray_columns_size(uint32_t width,uint32_t height)
{
    if (!width || width>AVP_WORLD_SCENE_MAX_WIDTH || !height ||
        height>AVP_WORLD_SCENE_MAX_HEIGHT)return 0u;
    return AVP_WORLD_GPU_WIRE_HEADER_BYTES +
        (size_t)width*(AVP_WORLD_GPU_WIRE_COLUMN_BYTES+AVP_WORLD_GPU_WIRE_HIT_BYTES);
}
int avp_vk_diag_ray_columns_make(uint8_t *data,size_t capacity,
                                uint32_t width,uint32_t height)
{
    const size_t needed=avp_vk_diag_ray_columns_size(width,height);
    const uint32_t hit_off=AVP_WORLD_GPU_WIRE_HEADER_BYTES+
             width*AVP_WORLD_GPU_WIRE_COLUMN_BYTES;
    if(!data || !needed || capacity<needed || capacity>32u*1024u*1024u)return 0;
    memset(data,0,needed);
    le32(data,AVP_WORLD_GPU_WIRE_MAGIC);
    le32(data+4u,AVP_WORLD_GPU_WIRE_VERSION);
    le32(data+8u,width);
    le32(data+12u,height);
    le32(data+16u,width);
    le32(data+20u,AVP_WORLD_GPU_WIRE_HEADER_BYTES);
    le32(data+24u,hit_off);
    for(uint32_t x=0u;x<width;++x){
        uint8_t *c=data+AVP_WORLD_GPU_WIRE_HEADER_BYTES+
                    (size_t)x*AVP_WORLD_GPU_WIRE_COLUMN_BYTES;
        uint8_t *h=data+hit_off+(size_t)x*AVP_WORLD_GPU_WIRE_HIT_BYTES;
        le32(c,x);              /* each column has one ordered hit */
        le32(c+4u,1u);
        le_double(c+8u,1.0);    /* positive finite depth */
        le32(c+16u,1u);         /* ready */
        le_double(h,1.0);
        le_double(h+8u,0.5);
        le32(h+16u,1u+((x/8u)&1u)); /* alternating eight-column wall-ID groups */
        le32(h+24u,1u);         /* vertical */
        le32(h+28u,1u);         /* final hit */
        le32(h+32u,0u);         /* clip low */
        le32(h+36u,height);     /* clip high */
    }
    return 1;
}
uint32_t avp_vk_diag_ray_columns_color(uint32_t x,uint32_t y)
{
    const uint32_t shift=(x/8u)&1u;
    return ((((x+shift)^y)&1u)!=0u) ? 0x00336699u : 0x00bb8855u;
}
