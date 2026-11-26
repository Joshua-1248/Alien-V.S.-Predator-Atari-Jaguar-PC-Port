/* RE47 R338: source-owned integer image staging for visible opaque walls.
 * This is an opt-in SDK-free preparation bridge, not active Hardware mode. */
#include "pc/pc_vulkan_wall_source_upload.h"
#include <limits.h>
#include <string.h>

static int avp_vk_wall_source_preflight(const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t per_limit, uint64_t total_limit, uint64_t *total_out)
{
    uint32_t expected_count=0u, columns=0u;
    uint64_t total=0u;
    if (!plan || !sources || !total_out || !per_limit || !total_limit ||
        plan->used_count>AVP_WORLD_SCENE_IMAGE_SLOTS ||
        plan->active_columns>AVP_WORLD_SCENE_MAX_WIDTH) return 0;
    for (uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++) {
        const AvpVkWallSlot *meta=&plan->slots[slot];
        const AvpGpuRgbImage *src;
        size_t count,bytes;
        uint32_t encoding;
        if (!meta->used) {
            if (meta->columns || meta->width || meta->height || meta->encoding)
                return 0;
            continue;
        }
        if (meta->used!=1u || expected_count>=plan->used_count ||
            plan->order[expected_count]!=slot || !meta->columns ||
            meta->columns>plan->active_columns-columns) return 0;
        columns+=meta->columns;
        expected_count++;
        src=sources[slot];
        if (!src || !src->rgb || src->width<=0 || src->height<=0 ||
            src->width>8192 || src->height>8192 ||
            meta->width!=(uint32_t)src->width ||
            meta->height!=(uint32_t)src->height) return 0;
        encoding=src->cry16 ? AVP_WORLD_IMAGE_CRY16_ALPHA8 :
                              AVP_WORLD_IMAGE_RGB8_ALPHA8;
        if (meta->encoding!=encoding) return 0;
        count=(size_t)src->width*(size_t)src->height;
        /* Multiplication is safe: both source dimensions <= 8192. */
        bytes=count*sizeof(uint32_t);
        if (!count || bytes>per_limit ||
            (uint64_t)bytes>total_limit-total) return 0;
        /* A wall with any alpha != 255 needs exact coverage/compositing,
         * which the current opaque-only fragment diagnostic cannot do. */
        if (src->alpha)
            for (size_t i=0u;i<count;i++)
                if (src->alpha[i]!=255u) return 0;
        total+=(uint64_t)bytes;
    }
    if (expected_count!=plan->used_count || columns!=plan->active_columns)
        return 0;
    *total_out=total;
    return 1;
}

int avp_vk_wall_source_upload(const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpWorldGpuStaging *scratch,AvpVkWallTextureUploadFn upload,
    void *user,AvpVkWallUploadStats *stats)
{
    AvpVkWallUploadStats temp={0};
    uint64_t expected_bytes=0u;
    if (stats)memset(stats,0,sizeof(*stats));
    if (scratch)avp_world_gpu_staging_invalidate(scratch);
    if (!stats || !scratch || !upload ||
        !avp_vk_wall_source_preflight(plan,sources,max_texture_bytes,
                                      max_total_bytes,&expected_bytes)) return 0;
    for (uint32_t i=0u;i<plan->used_count;i++) {
        const uint32_t slot=plan->order[i];
        const AvpVkWallSlot *meta=&plan->slots[slot];
        if (!avp_world_gpu_stage_image(scratch,sources[slot],max_texture_bytes) ||
            !scratch->valid || scratch->width!=meta->width ||
            scratch->height!=meta->height || scratch->encoding!=meta->encoding ||
            !upload(user,slot,scratch->words,scratch->width,scratch->height,
                    scratch->encoding,scratch->byte_count)) {
            /* The caller owns, and must roll back, provisional GPU objects.
             * This function never publishes an incomplete texture set. */
            avp_world_gpu_staging_invalidate(scratch);
            return 0;
        }
        ++temp.slots_uploaded;
        temp.source_bytes_uploaded+=(uint64_t)scratch->byte_count;
    }
    avp_world_gpu_staging_invalidate(scratch);
    if (temp.source_bytes_uploaded!=expected_bytes) return 0;
    *stats=temp;
    return 1;
}
