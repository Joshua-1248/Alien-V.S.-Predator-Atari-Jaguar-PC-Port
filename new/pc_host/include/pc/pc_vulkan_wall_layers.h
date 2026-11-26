#ifndef AVP_PC_VULKAN_WALL_LAYERS_H
#define AVP_PC_VULKAN_WALL_LAYERS_H
/* RE47/R357: backend-neutral original MAZE2 layered wall/door/portal spans.
 * Transmits source image coordinates, NOT Software-rendered frame pixels.
 * Each column's spans are ordered back-to-front, with exact CPU-source row
 * indices for the future Vulkan blend/depth pass. The active renderer is NOT
 * changed by this module. */
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/world_scene_packet.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_VK_WALL_LAYER_MAX_SPANS 131072u
#define AVP_VK_WALL_LAYER_MAX_ROWS (4u*1024u*1024u)
#define AVP_VK_WALL_LAYER_NO_IMAGE UINT32_MAX
/* Host record; serialize explicitly before GPU upload (not std430). */
typedef struct AvpVkWallLayerSpan {
    uint32_t column,source_slot,wall_code,source_x;
    uint32_t top,bottom,row_offset;
    int32_t cry_shade;
    float distance;
} AvpVkWallLayerSpan;
typedef struct AvpVkWallLayers {
    uint32_t width,height,span_count,span_capacity,row_count,row_capacity;
    uint32_t *column_start; /* width+1 offsets into spans */
    AvpVkWallLayerSpan *spans;
    uint16_t *rows; /* exact texel row, one per visible span pixel */
    uint32_t doorway_columns,window_columns,portal_columns;
} AvpVkWallLayers;
/* All-or-nothing source capture. Rejects unsupported AF geometry filtering,
 * malformed hit order, invalid source metadata, geometry overflow and any
 * other condition that could hide original gameplay geometry. */
int avp_vk_wall_layers_build(const AvpWorldScenePacket *scene,
                             AvpVkWallLayers *out);
/* Verifies offsets, layer order, span geometry and row ownership. */
int avp_vk_wall_layers_validate(const AvpVkWallLayers *layer,
                                const AvpWorldScenePacket *scene);
/* Returns the final geometric texel at x/y (ignores source alpha). A later
 * actual Vulkan composition pass MUST resolve alpha/depth before presentation. */
int avp_vk_wall_layers_lookup(const AvpVkWallLayers *layer,uint32_t x,
                               uint32_t y,uint32_t *slot,uint32_t *source_x,
                               uint32_t *source_y,int32_t *shade);
void avp_vk_wall_layers_dispose(AvpVkWallLayers *layer);
#ifdef __cplusplus
}
#endif
#endif
