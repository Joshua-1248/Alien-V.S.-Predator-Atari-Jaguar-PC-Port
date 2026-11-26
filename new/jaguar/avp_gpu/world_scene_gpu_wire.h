#ifndef AVP_JAGUAR_AVP_GPU_WORLD_SCENE_GPU_WIRE_H
#define AVP_JAGUAR_AVP_GPU_WORLD_SCENE_GPU_WIRE_H
/* R331: independent GPU upload stream format for CPU-authored ray hits.
 * All integers are unsigned little endian on the wire; IEEE-754 doubles are
 * encoded as two u32 words (least-significant first), never as host structs.
 * A shader may reconstruct the 64-bit original bit pattern, or consume an
 * independently verified reduced-precision representation in a later stage.
 * No ROM data, shader binaries or driver implementation is bundled. */
#include <stdint.h>
#include <stddef.h>
#include "jaguar/avp_gpu/world_scene_packet.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_WORLD_GPU_WIRE_MAGIC 0x57525641u /* 'AVRW' bytes */
#define AVP_WORLD_GPU_WIRE_VERSION 1u
#define AVP_WORLD_GPU_WIRE_HEADER_BYTES 32u
#define AVP_WORLD_GPU_WIRE_COLUMN_BYTES 24u
#define AVP_WORLD_GPU_WIRE_HIT_BYTES 40u
/* Stream contains geometry only, not textures or entire software pixels. */
typedef struct AvpWorldGpuWire {
    uint8_t *data;
    size_t capacity;
    size_t bytes; /* zero means invalid, not a usable GPU resource */
} AvpWorldGpuWire;
void avp_world_gpu_wire_destroy(AvpWorldGpuWire *dst);
void avp_world_gpu_wire_invalidate(AvpWorldGpuWire *dst);
/* Returns 1 only for a complete, finite, internally consistent scene.
 * On failure, dst->bytes is always zero; retained allocation may be reused.
 * No allocations or writes occur while the original ray worker is running. */
int avp_world_gpu_wire_pack(AvpWorldGpuWire *dst,
                            const AvpWorldScenePacket *packet);
#ifdef __cplusplus
}
#endif
#endif
