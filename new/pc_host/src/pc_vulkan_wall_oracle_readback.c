/* R347: Vulkan-only GPU readback bridge. Official SDK is required.
 * Kept separate to leave R330 mock tests and the Software game untouched. */
#include "pc/pc_vulkan_readback.h"
#include <limits.h>
/* R347: map the real GPU copy (never a Software frame upload) and compare
 * against the independently retained source/geometry frame snapshot. */
int avp_vk_readback_verify_wall_capture(VkDevice device,
    const AvpVkReadback *rb,const AvpVkWallOracleCapture *capture,
    uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!device || !rb || !rb->memory || !capture || !mismatches ||
       !capture->wire || capture->wire_bytes<32u)return 0;
    const uint8_t *wire=capture->wire;
    uint32_t width=(uint32_t)wire[8]|((uint32_t)wire[9]<<8u)|
                   ((uint32_t)wire[10]<<16u)|((uint32_t)wire[11]<<24u);
    uint32_t height=(uint32_t)wire[12]|((uint32_t)wire[13]<<8u)|
                    ((uint32_t)wire[14]<<16u)|((uint32_t)wire[15]<<24u);
    if(width!=rb->width || height!=rb->height ||
       (uint64_t)width*height*4u!=rb->bytes)return 0;
    if(vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_wall_oracle_capture_compare(capture,(uint32_t)rb->format,
        mapped,(size_t)rb->bytes,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}
