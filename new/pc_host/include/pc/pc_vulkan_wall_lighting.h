#ifndef AVP_PC_VULKAN_WALL_LIGHTING_H
#define AVP_PC_VULKAN_WALL_LIGHTING_H
/* R341. Encode original MAZE2 distance shade as a 12th word per wall column.
 * Does not change Software renderer or v1 diagnostic packets. */
#include "pc/pc_vulkan_wall_projection.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Calls the exact original host Jaguar projection/shading implementation. */
int avp_vk_retail_wall_shade(double distance,int fog_mode,int disable_fog);
/* Uses v1 ray/projection validation; emits v2 lit records atomically.
 * RGB replacements stay unshaded, exactly as Software's sample_tex_shaded.
 * Source CRY16 gets the original Jaguar distance/intensity delta. */
int avp_vk_wall_projection_pack_lit(AvpVkWallProjectionStream *out,
                                    const AvpWorldScenePacket *scene);
#ifdef __cplusplus
}
#endif
#endif
