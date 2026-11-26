/* RE47 R340: original ray packet + original source image -> backend-neutral
 * validated GPU draw frame. The normal Software game never calls this. */
#include "pc/pc_vulkan_wall_live_frame.h"
#include "pc/pc_vulkan_wall_lighting.h"
#include <string.h>

int avp_vk_wall_live_frame_dispose(AvpVkWallLiveFrame *frame,
    AvpVkWallImageReleaseFn release,void *user)
{
    if(!frame || (frame->images.count && !release))return 0;
    avp_vk_wall_image_bank_dispose(&frame->images,release,user);
    avp_vk_wall_projection_destroy(&frame->geometry);
    memset(&frame->draws,0,sizeof(frame->draws));
    return 1;
}

int avp_vk_wall_live_prepare(const AvpWorldScenePacket *scene,
    const AvpAvpGpuState *state,AvpVkWallProjectionStream *geometry,
    AvpVkWallSlotPlan *slots,
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS])
{
    AvpVkWallProjectionStream candidate={0};
    AvpVkWallSlotPlan selected={0};
    const AvpGpuRgbImage *selected_sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint32_t differences=UINT32_MAX;
    if(!scene || !state || !geometry || !slots || !sources ||
       geometry->data || geometry->bytes || geometry->capacity)return 0;
    /* No consumer may inherit an old plan after an unsuccessful build. */
    memset(slots,0,sizeof(*slots));
    memset(sources,0,sizeof(selected_sources));
    if(!avp_vk_wall_projection_pack_lit(&candidate,scene) ||
       !avp_vk_wall_projection_row_parity(candidate.data,
                                         candidate.bytes,&differences))goto fail;
    if(differences && !avp_vk_wall_projection_upgrade_exact_rows(
                           &candidate,NULL,NULL))goto fail;
    differences=UINT32_MAX;
    if(!avp_vk_wall_projection_row_parity(candidate.data,candidate.bytes,
                                        &differences) || differences ||
       !avp_vk_wall_slots_collect(candidate.data,candidate.bytes,
                                  &selected))goto fail;
    for(uint32_t i=0;i<selected.used_count;i++){
        const uint32_t slot=selected.order[i];
        const AvpGpuRgbImage *image=avp_world_scene_source(state,slot);
        /* Recheck original/hi-res source metadata BEFORE any GPU work. */
        if(!image || image->width<=0 || image->height<=0 ||
           selected.slots[slot].width!=(uint32_t)image->width ||
           selected.slots[slot].height!=(uint32_t)image->height ||
           selected.slots[slot].encoding!=(image->cry16 ?
                   AVP_WORLD_IMAGE_CRY16_ALPHA8 :
                   AVP_WORLD_IMAGE_RGB8_ALPHA8))goto fail;
        selected_sources[slot]=image;
    }
    *geometry=candidate;
    *slots=selected;
    memcpy(sources,selected_sources,sizeof(selected_sources));
    return 1;
fail:
    avp_vk_wall_projection_destroy(&candidate);
    return 0;
}

int avp_vk_wall_live_frame_build(const AvpWorldScenePacket *scene,
    const AvpAvpGpuState *state,size_t max_texture_bytes,
    uint64_t max_total_bytes,AvpWorldGpuStaging *scratch,
    AvpVkWallImageAcquireFn acquire,AvpVkWallImageReleaseFn release,
    void *user,AvpVkWallLiveFrame *out)
{
    AvpVkWallLiveFrame candidate={0};
    AvpVkWallSlotPlan slots={0};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    if(!out || !scene || !state || !scratch || !acquire || !release ||
       out->geometry.data || out->geometry.bytes || out->geometry.capacity ||
       out->images.count || out->images.uploaded_bytes ||
       out->draws.count || out->draws.active_columns ||
       out->draws.width || out->draws.height)return 0;
    for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++)
        if(out->images.slots[slot].resource || out->images.slots[slot].width ||
           out->images.slots[slot].height || out->images.slots[slot].encoding ||
           out->images.order[slot] || out->draws.draws[slot].columns ||
           out->draws.draws[slot].slot || out->draws.draws[slot].width ||
           out->draws.draws[slot].height || out->draws.draws[slot].encoding)
            return 0;
    if(!avp_vk_wall_live_prepare(scene,state,&candidate.geometry,
                                  &slots,sources))goto fail;
    if(!avp_vk_wall_image_bank_build(&slots,sources,max_texture_bytes,
            max_total_bytes,scratch,acquire,release,user,&candidate.images) ||
       !avp_vk_wall_draw_batch_build(candidate.geometry.data,
                candidate.geometry.bytes,&candidate.images,&candidate.draws))goto fail;
    *out=candidate;
    return 1;
fail:
    (void)avp_vk_wall_live_frame_dispose(&candidate,release,user);
    avp_world_gpu_staging_invalidate(scratch);
    return 0;
}
