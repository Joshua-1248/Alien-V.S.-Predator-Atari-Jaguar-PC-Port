/* RE47 R347: source-backed, SDK-independent per-frame GPU oracle capture. */
#include "pc/pc_vulkan_wall_oracle_capture.h"
#include <stdlib.h>
#include <string.h>

void avp_vk_wall_oracle_capture_dispose(AvpVkWallOracleCapture *capture)
{
    if(!capture)return;
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        free(capture->owned_words[i]);
    free(capture->owned_cry_lut);
    free(capture->wire);
    memset(capture,0,sizeof(*capture));
}
static int capture_empty(const AvpVkWallOracleCapture *c)
{
    if(c->wire || c->wire_bytes || c->texture_count || c->texture_bytes ||
       c->owned_cry_lut)return 0;
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if(c->owned_words[i] || c->sources[i].words || c->sources[i].width ||
           c->sources[i].height || c->sources[i].encoding)return 0;
    return 1;
}
int avp_vk_wall_oracle_capture_build(const uint8_t *wire,size_t wire_bytes,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    const uint32_t *cry_lut,size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpVkWallOracleCapture *out)
{
    AvpVkWallOracleCapture candidate={0};
    AvpVkWallSlotPlan plan={0};
    AvpWorldGpuStaging staging={0};
    int ok=0;
    if(!out || !capture_empty(out) || !sources || !max_texture_bytes ||
       !max_total_bytes || !avp_vk_wall_slots_collect(wire,wire_bytes,&plan) ||
       wire_bytes>AVP_VK_WALL_PROJECTION_MAX_EXACT_BYTES ||
       (plan.active_columns && !plan.used_count))return 0;
    candidate.wire=malloc(wire_bytes);
    if(!candidate.wire)goto done;
    memcpy(candidate.wire,wire,wire_bytes);
    candidate.wire_bytes=wire_bytes;
    for(uint32_t j=0u;j<plan.used_count;j++){
        uint32_t slot=plan.order[j];
        const AvpVkWallSlot *meta=&plan.slots[slot];
        const AvpGpuRgbImage *src=sources[slot];
        size_t count;
        if(!src || !src->rgb || src->width<=0 || src->height<=0 ||
           meta->width!=(uint32_t)src->width ||
           meta->height!=(uint32_t)src->height ||
           meta->encoding!=(src->cry16 ? AVP_WORLD_IMAGE_CRY16_ALPHA8 :
                                        AVP_WORLD_IMAGE_RGB8_ALPHA8))goto done;
        count=(size_t)src->width*(size_t)src->height;
        if(src->alpha){
            for(size_t i=0;i<count;i++)if(src->alpha[i]!=255u)goto done;
        }
        if(!avp_world_gpu_stage_image(&staging,src,max_texture_bytes) ||
           !staging.valid || staging.width!=meta->width ||
           staging.height!=meta->height || staging.encoding!=meta->encoding ||
           staging.byte_count>max_total_bytes-candidate.texture_bytes)
            goto done;
        candidate.owned_words[slot]=malloc(staging.byte_count);
        if(!candidate.owned_words[slot])goto done;
        memcpy(candidate.owned_words[slot],staging.words,staging.byte_count);
        candidate.sources[slot]=(AvpVkWallPixelSource){
            candidate.owned_words[slot],staging.width,staging.height,
            staging.encoding};
        candidate.texture_bytes+=staging.byte_count;
        ++candidate.texture_count;
        if(staging.encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
           !candidate.owned_cry_lut){
            if(!cry_lut)goto done;
            candidate.owned_cry_lut=malloc(65536u*sizeof(uint32_t));
            if(!candidate.owned_cry_lut)goto done;
            memcpy(candidate.owned_cry_lut,cry_lut,65536u*sizeof(uint32_t));
        }
    }
    *out=candidate;
    memset(&candidate,0,sizeof(candidate));
    ok=1;
done:
    avp_world_gpu_staging_dispose(&staging);
    avp_vk_wall_oracle_capture_dispose(&candidate);
    return ok;
}
int avp_vk_wall_oracle_capture_compare(const AvpVkWallOracleCapture *capture,
    uint32_t swapchain_format,const uint8_t *gpu_pixels,size_t gpu_bytes,
    uint32_t *mismatches)
{
    if(mismatches)*mismatches=UINT32_MAX;
    if(!capture || !capture->wire || capture->wire_bytes<32u ||
       !mismatches || !gpu_pixels || !capture->texture_count)return 0;
    /* A frame is not pixel-identical if the GPU copied a differently sized
     * swapchain image. Require exact bytes, not a matching prefix. */
    const uint8_t *w=capture->wire;
    uint32_t width=(uint32_t)w[8]|((uint32_t)w[9]<<8u)|
                   ((uint32_t)w[10]<<16u)|((uint32_t)w[11]<<24u);
    uint32_t height=(uint32_t)w[12]|((uint32_t)w[13]<<8u)|
                    ((uint32_t)w[14]<<16u)|((uint32_t)w[15]<<24u);
    if((uint64_t)width*height*4u!=gpu_bytes)return 0;
    return avp_vk_wall_pixel_oracle_compare(capture->wire,capture->wire_bytes,
        capture->sources,capture->owned_cry_lut,swapchain_format,
        gpu_pixels,gpu_bytes,mismatches);
}
