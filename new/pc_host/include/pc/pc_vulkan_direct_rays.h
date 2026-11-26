#ifndef AVP_PC_VULKAN_DIRECT_RAYS_H
#define AVP_PC_VULKAN_DIRECT_RAYS_H
/* R382 GPU-first world-ray seam. Both inputs MUST be complete, independently
 * owned source packets from the same scene. Never compare pointer addresses,
 * struct padding, stale columns or CPU framebuffer colors. */
#include "jaguar/avp_gpu/world_scene_packet.h"
#ifdef __cplusplus
extern "C" {
#endif
/* True only for byte-/scalar-identical ORIGINAL ray/texture scene semantics.
 * The CPU Software pass is an offline/runtime oracle for this transition,
 * not a required input of a future GPU-first source generator. */
int avp_vk_direct_rays_match_capture(const AvpWorldScenePacket *direct,
                                     const AvpWorldScenePacket *captured);
#ifdef __cplusplus
}
#endif
#endif
