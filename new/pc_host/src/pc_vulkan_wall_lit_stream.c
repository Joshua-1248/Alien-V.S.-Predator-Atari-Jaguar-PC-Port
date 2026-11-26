/* R341: binary-compatible v1 projected columns gain a separately versioned
 * shade word. No pixel rasterization or CPU framebuffer upload occurs. */
#include "pc/pc_vulkan_wall_lighting.h"
#include "jaguar/avp_gpu/texture_filter.h"
#include <stdlib.h>
#include <string.h>

static void store32(uint8_t *p,uint32_t value)
{
    p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8u);
    p[2]=(uint8_t)(value>>16u);p[3]=(uint8_t)(value>>24u);
}
int avp_vk_wall_projection_pack_lit(AvpVkWallProjectionStream *out,
                                    const AvpWorldScenePacket *scene)
{
    AvpVkWallProjectionStream legacy={0};
    uint8_t *new_data;
    size_t bytes;
    int ok=0;
    if(!out)return 0;
    out->bytes=0u;
    if(!scene || scene->disable_fog>1u || scene->fog_distance_mode>=4u ||
       /* The current Vulkan wall shader implements nearest-point sampling,
        * not the Software renderer's geometry bilinear/mip/aniso modes.
        * Reject such frames instead of presenting visibly different art. */
       ((scene->filter_mask & (1u<<AVP_FILTER_GEOMETRY)) &&
        (scene->filter_mode!=AVP_TEXFILTER_NEAREST ||
         scene->anisotropic_taps>1u)) ||
       !avp_vk_wall_projection_pack(&legacy,scene))goto done;
    bytes=AVP_VK_WALL_PROJECTION_HEADER_BYTES+
          (size_t)scene->width*AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES;
    if(bytes>1024u*1024u)goto done;
    if(out->capacity<bytes){
        new_data=(uint8_t *)realloc(out->data,bytes);
        if(!new_data)goto done;
        out->data=new_data;out->capacity=bytes;
    }
    memcpy(out->data,legacy.data,AVP_VK_WALL_PROJECTION_HEADER_BYTES);
    store32(out->data+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    store32(out->data+16u,AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES);
    for(uint32_t x=0u;x<scene->width;x++){
        const uint8_t *old=legacy.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
            (size_t)x*AVP_VK_WALL_PROJECTION_COLUMN_BYTES;
        uint8_t *record=out->data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
            (size_t)x*AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES;
        int intensity=0;
        memcpy(record,old,AVP_VK_WALL_PROJECTION_COLUMN_BYTES);
        if(old[0]){
            const AvpWorldRayHit *hit=scene->hits+
                (size_t)x*scene->hit_capacity_per_column;
            const uint32_t slot=scene->wall_image[hit->wall];
            /* Software does not shade imported RGB replacements; only
             * CRY16 originals receive the GPU's CRY intensity adjustment. */
            if(scene->image[slot].encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8){
                const double distance=hit->distance<0.001?0.001:hit->distance;
                intensity=avp_vk_retail_wall_shade(distance,
                    (int)scene->fog_distance_mode,(int)scene->disable_fog);
                if(intensity < -255 || intensity > 255)goto done;
            }
        }
        store32(record+44u,(uint32_t)(int32_t)intensity);
    }
    if(!avp_vk_wall_projection_validate(out->data,bytes))goto done;
    out->bytes=bytes;
    ok=1;
done:
    avp_vk_wall_projection_destroy(&legacy);
    return ok;
}
