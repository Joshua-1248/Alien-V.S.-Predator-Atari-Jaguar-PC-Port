#ifndef AVP_JAGUAR_AVP_GPU_WORLD_SCENE_PACKET_H
#define AVP_JAGUAR_AVP_GPU_WORLD_SCENE_PACKET_H
/* RE #46 R323: host-only, backend-neutral world-scene transfer contract.
 * Original Jaguar simulation/rendering is NOT replaced. No Vulkan SDK needed.
 * All bytes referenced here are decoded at runtime from the user's ROM or
 * independently supplied replacement textures; no cartridge art is embedded.
 */
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/bridge.h"
#ifdef __cplusplus
extern "C" {
#endif

#define AVP_WORLD_SCENE_INVALID_SLOT UINT16_MAX
#define AVP_WORLD_SCENE_PANELS 74u
#define AVP_WORLD_SCENE_SIGNS 18u
#define AVP_WORLD_SCENE_IMAGE_SLOTS (AVP_WORLD_SCENE_PANELS+AVP_WORLD_SCENE_SIGNS)
#define AVP_WORLD_SCENE_MAX_PORTALS 48u
#define AVP_WORLD_SCENE_MAX_HITS (AVP_WORLD_SCENE_MAX_PORTALS+1u)
#define AVP_WORLD_SCENE_MAX_WIDTH 8192u
#define AVP_WORLD_SCENE_MAX_HEIGHT 8192u
#define AVP_WORLD_SCENE_MAX_MAP_DIM 256u

/* The GPU will need an explicit shader for the original CRY16 RMW and
 * alpha coverage. The host does not reinterpret CRY16 as ordinary RGB. */
typedef enum AvpWorldImageEncoding {
    AVP_WORLD_IMAGE_ABSENT=0,
    AVP_WORLD_IMAGE_CRY16_ALPHA8=1, /* R32_UINT: bits 0..15 CRY, 16..23 alpha */
    AVP_WORLD_IMAGE_RGB8_ALPHA8=2   /* R32_UINT: bits 0..23 RGB, 24..31 alpha */
} AvpWorldImageEncoding;

typedef struct AvpWorldImageMeta {
    uint32_t width, height, encoding;
} AvpWorldImageMeta;

typedef struct AvpWorldRayHit {
    double distance, texture_fraction; /* CPU-original ray values, unrounded */
    uint32_t wall, open, vertical, is_final;
    int32_t clip_lo, clip_hi;
} AvpWorldRayHit;

typedef struct AvpWorldRayColumn {
    double wall_depth; /* the exact sprite-occlusion z buffer value */
    uint32_t count, ready, overflow;
} AvpWorldRayColumn;

/* CPU-side staging structure, NOT a Vulkan std430 binary layout. A future
 * Vulkan module must serialize with an explicitly specified GPU layout. */
typedef struct AvpWorldScenePacket {
    uint32_t width, height, logical_level, map_width, map_height;
    uint32_t image_slot_count, hit_capacity_per_column;
    uint32_t filter_mode, anisotropic_taps, filter_mask;
    uint32_t occlusion_enabled, disable_fog, draw_distance_mode, fog_distance_mode;
    uint32_t editor_world_only;
    double editor_pitch;
    double camera_x, camera_y, forward_x, forward_y;
    double right_x, right_y, focal, stereo_shift;
    uint16_t wall_image[256], flat_image[256], wall_window_pair[256];
    uint8_t flat_rotation[256];
    uint8_t window_pairs[13][256];
    uint8_t window_pair_loaded[13];
    AvpWorldImageMeta image[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint8_t *maze; size_t maze_capacity;
    AvpWorldRayHit *hits;
    AvpWorldRayColumn *columns;
} AvpWorldScenePacket;

struct AvpAvpGpuState;
int avp_world_scene_init(AvpWorldScenePacket *packet,uint32_t width,uint32_t height);
void avp_world_scene_destroy(AvpWorldScenePacket *packet);
void avp_world_scene_invalidate(AvpWorldScenePacket *packet);
/* Begin is called with the FINAL presentation maze (including optionally
 * interpolated doors), and FINAL camera pose used by the software renderer.
 * Returns 0 without exposing a partially valid packet on invalid input. */
int avp_world_scene_begin(AvpWorldScenePacket *packet,
                          const struct AvpAvpGpuState *state,
                          const uint8_t *maze,uint32_t map_width,uint32_t map_height,
                          uint32_t level,double camera_x,double camera_y,
                          double forward_x,double forward_y,double right_x,double right_y,
                          double focal);
/* Exactly one worker owns each column; all writes are to separate memory.
 * No global counters, allocation, or locks in the raster worker. */
int avp_world_scene_hit(AvpWorldScenePacket *packet,uint32_t x,
                        double distance,double texture_fraction,
                        uint32_t wall,uint32_t open,uint32_t vertical,uint32_t is_final,
                        int32_t clip_lo,int32_t clip_hi);
void avp_world_scene_end_column(AvpWorldScenePacket *packet,uint32_t x,double depth);
int avp_world_scene_complete(const AvpWorldScenePacket *packet);
/* Source pointers are borrowed only during synchronous upload preparation;
 * never retain them in the Vulkan driver or serialize them as handles. */
const AvpGpuRgbImage *avp_world_scene_source(const struct AvpAvpGpuState *state,
                                             uint32_t image_slot);
/* Exact source texel bits + alpha for Vulkan integer textures.  The caller
 * supplies at least width*height uint32 words. No malloc/ROM decoding here.
 * No ARGB conversion for CRY16 pixels: the shader must apply Jaguar shade. */
int avp_world_scene_pack_image(const AvpGpuRgbImage *image,
                               uint32_t *out_words,size_t word_capacity,
                               AvpWorldImageEncoding *out_encoding);
#ifdef __cplusplus
}
#endif
#endif
