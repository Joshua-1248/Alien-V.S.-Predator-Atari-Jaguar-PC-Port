/* RE47 R340. No new raycaster, shading or GPU framebuffer capture. */
#include "pc/pc_vulkan_wall_draw_batch.h"
#include <string.h>

static uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
int avp_vk_wall_draw_batch_build(const uint8_t *wire,size_t bytes,
    const AvpVkWallImageBank *bank,AvpVkWallDrawBatch *out)
{
    AvpVkWallSlotPlan plan={0};
    AvpVkWallDrawBatch draft={0};
    uint64_t image_bytes=0u;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!bank || !avp_vk_wall_slots_collect(wire,bytes,&plan) ||
       plan.used_count!=bank->count ||
       (plan.used_count==0u && bank->uploaded_bytes!=0u))return 0;
    draft.width=load32(wire+8u);
    draft.height=load32(wire+12u);
    draft.active_columns=plan.active_columns;
    for(uint32_t i=0u;i<plan.used_count;i++) {
        const uint32_t slot=plan.order[i];
        const AvpVkWallSlot *src=&plan.slots[slot];
        const AvpVkWallImageEntry *image=&bank->slots[slot];
        if(bank->order[i]!=slot || !image->resource ||
           src->width!=image->width || src->height!=image->height ||
           src->encoding!=image->encoding)return 0;
        for(uint32_t j=0u;j<i;j++)
            if(bank->slots[plan.order[j]].resource==image->resource)return 0;
        image_bytes+=(uint64_t)image->width*image->height*sizeof(uint32_t);
        draft.draws[i]=(AvpVkWallDraw){slot,src->columns,src->width,
                                     src->height,src->encoding};
    }
    for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++) {
        if(!plan.slots[slot].used && (bank->slots[slot].resource ||
           bank->slots[slot].width || bank->slots[slot].height ||
           bank->slots[slot].encoding))return 0;
    }
    if(image_bytes!=bank->uploaded_bytes)return 0;
    draft.count=plan.used_count;
    *out=draft;
    return 1;
}
