/* RE47 R337: robust per-texture Vulkan wall draw ordering. CPU scene
 * semantics remain authoritative; Software rendering is untouched. */
#include "pc/pc_vulkan_wall_slots.h"
#include <string.h>

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
int avp_vk_wall_slots_collect(const uint8_t *wire,size_t bytes,
                              AvpVkWallSlotPlan *plan)
{
    AvpVkWallSlotPlan tmp={0};
    uint32_t width,stride;
    if(!plan)return 0;
    memset(plan,0,sizeof(*plan));
    if(!avp_vk_wall_projection_validate(wire,bytes))return 0;
    width=le32(wire+8u);
    stride=le32(wire+16u);
    for(uint32_t x=0u;x<width;x++) {
        const uint8_t *rec=wire+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
            (size_t)x*stride;
        if(!le32(rec))continue;
        uint32_t slot=le32(rec+4u),tw=le32(rec+32u),
                 th=le32(rec+36u),encoding=le32(rec+40u);
        AvpVkWallSlot *s=&tmp.slots[slot];
        if(s->used){
            if(s->width!=tw || s->height!=th || s->encoding!=encoding)
                return 0;
        }else {
            s->used=1u;s->width=tw;s->height=th;s->encoding=encoding;
            ++tmp.used_count;
        }
        ++s->columns;
        ++tmp.active_columns;
    }
    if(tmp.active_columns!=le32(wire+24u))return 0;
    uint32_t order_index=0u;
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if(tmp.slots[i].used)
            tmp.order[order_index++]=i;
    *plan=tmp;
    return 1;
}
