#ifndef AVP_PC_VULKAN_WALL_PROJECTION_H
#define AVP_PC_VULKAN_WALL_PROJECTION_H
/* RE46 R336: independently authored, opt-in *opaque plain wall* GPU layout.
 * Geometry is derived from original CPU ray hits; NO framebuffer pixels.
 * The first pass explicitly rejects doors, windows, portals, fog/AF parity
 * requirements and other unsupported cases instead of misrendering them.
 * The stream is not an active Hardware backend. */
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/world_scene_packet.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_VK_WALL_PROJECTION_MAGIC 0x4c575641u /* bytes 'AVWL' */
#define AVP_VK_WALL_PROJECTION_VERSION 1u /* diagnostic v1: unshaded */
#define AVP_VK_WALL_PROJECTION_LIT_VERSION 2u /* R341: per-column Jaguar shade */
#define AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES 48u
/* R345: 52-byte lit columns + optional packed u16 exact high-res row maps.
 * A map word holds rows for two screen pixels (low 16 first). The reserved
 * header word stores the first map word index; a zero column offset means
 * the original integer sampling already matches the Software renderer. */
#define AVP_VK_WALL_PROJECTION_EXACT_VERSION 3u
#define AVP_VK_WALL_PROJECTION_EXACT_COLUMN_BYTES 52u
#define AVP_VK_WALL_PROJECTION_MAX_EXACT_BYTES (32u*1024u*1024u)
#define AVP_VK_WALL_PROJECTION_HEADER_BYTES 32u
#define AVP_VK_WALL_PROJECTION_COLUMN_BYTES 44u
#define AVP_VK_WALL_PROJECTION_BASE_BIAS 32768u
#define AVP_VK_WALL_PROJECTION_MAX_HEIGHT 32768u
/* Header (all u32 little endian): magic, version, width, height,
 * record stride=44, records offset=32, supported count, reserved=0.
 * Column (11 u32): active, image slot, wall code, clipped top, clipped
 * bottom (exclusive), original top BIASED BY 32768 (never reinterpret a uint
 * as a signed shader integer), exact projected height, source x,
 * texture width, texture height, texture encoding. Signed values use
 * two's complement. No native compiler struct layout may be uploaded. */
typedef struct AvpVkWallProjectionStream {
    uint8_t *data;
    size_t capacity;
    size_t bytes;
} AvpVkWallProjectionStream;
void avp_vk_wall_projection_destroy(AvpVkWallProjectionStream *stream);
void avp_vk_wall_projection_invalidate(AvpVkWallProjectionStream *stream);
/* Complete-frame gate. Only plain solid walls (types 0,1,2,12),
 * one final hit per column, or an empty column, are supported.
 * Returns false and invalidates the output if any column requires
 * unsupported portals/windows/doors or has damaged source data. */
int avp_vk_wall_projection_pack(AvpVkWallProjectionStream *dst,
                                const AvpWorldScenePacket *scene);
/* Synthetic 16x16 checkerboard + single solid wall at distance=1.
 * Independent CPU expected-image oracle for optional GPU readback.
 * Only BGRA8=44 and RGBA8=37 are accepted. */
int avp_vk_wall_projection_check_readback(const uint8_t *rgba,size_t bytes,
    uint32_t width,uint32_t height,uint32_t format,uint32_t *mismatches);
/* R337 synthetic two-slot 16x16 image/GPU multi-draw parity oracle.
 * Unlike single-slot R336, alternating 16-column blocks bind slots 5/6;
 * this still does not certify real ROM images or full gameplay rendering. */
int avp_vk_wall_projection_check_two_slots(const uint8_t *rgba,size_t bytes,
    uint32_t width,uint32_t height,uint32_t format,uint32_t *mismatches);
/* V1/44-byte, v2/48-byte and v3/52-byte plus packed u16 row-tail streams
 * are supported. Version/stride combinations cannot be mixed; signed shade,
 * map ordering, bounds, and canonical odd-row padding are checked. */
/* Separate wire validator. No Vulkan SDK or live GPU needed. */
int avp_vk_wall_projection_validate(const uint8_t *data,size_t bytes);
/* Compare shader source-row selection (integer for v1/v2, optional exact
 * u16 lookup for v3) against Software double-precision row stepping from
 * the clipped first visible row. Valid streams may still have nonzero
 * mismatches; no CPU framebuffer pixels or artwork are read. */
int avp_vk_wall_projection_row_parity(const uint8_t *wire,size_t bytes,
                                    uint32_t *mismatches);
/* Upgrade a validated v2 lit wall stream transactionally. For high-resolution
 * columns whose integer GPU sampling differs from the Software renderer's
 * incremented double phase, append an exact two-u16-per-word texel-row map.
 * Unaffected streams remain v2 byte-for-byte. Out unchanged on failure. */
int avp_vk_wall_projection_upgrade_exact_rows(AvpVkWallProjectionStream *stream,
                                              uint32_t *mapped_columns,
                                              uint32_t *mapped_rows);
/* Resolve a visible pixel to the source row using either v1/v2 integer
 * sampling or v3 exact maps. Reject invalid streams / out-of-band rows. */
int avp_vk_wall_projection_lookup_row(const uint8_t *wire,size_t bytes,
                                      uint32_t column,uint32_t screen_row,
                                      int *source_row);
/* For CPU parity tests and later shader-reference testing. */
int avp_vk_wall_projection_sample_y(int screen_y,int base_top,
                                    int projected_height,int texture_height);
#ifdef __cplusplus
}
#endif
#endif
