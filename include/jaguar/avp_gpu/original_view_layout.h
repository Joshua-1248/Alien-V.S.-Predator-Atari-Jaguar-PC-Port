#ifndef AVP_JAGUAR_ORIGINAL_VIEW_LAYOUT_H
#define AVP_JAGUAR_ORIGINAL_VIEW_LAYOUT_H
/* RE24 S21: ROM-capture-addressed scene resources for a DISPOSABLE native-GPU
 * memory image. This is a loader, not proof of any particular retail address.
 * The caller must obtain resource address/bytes/hash independently from the
 * authentic ROM execution. Never auto-place resources into guessed addresses.
 * No changes to the live game or its renderer. */
#include "jaguar/avp_gpu/original_view_scene.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_GPU_NATIVE_MAX_SCENE_RESOURCES 64u

typedef struct AvpGpuNativeSceneResource {
    uint32_t dram_address; /* verified Jaguar DRAM destination, not a host pointer */
    const uint8_t *bytes;  /* caller-owned original-format resource bytes */
    size_t length;
    uint64_t expected_hash; /* caller-supplied FNV-1a byte hash, nonzero */
} AvpGpuNativeSceneResource;

typedef enum AvpGpuNativeLayoutResult {
    AVP_GPU_NATIVE_LAYOUT_OK = 0,
    AVP_GPU_NATIVE_LAYOUT_INVALID,
    AVP_GPU_NATIVE_LAYOUT_CAPACITY,
    AVP_GPU_NATIVE_LAYOUT_ALIAS,
    AVP_GPU_NATIVE_LAYOUT_CONFLICT,
    AVP_GPU_NATIVE_LAYOUT_HASH_MISMATCH,
    AVP_GPU_NATIVE_LAYOUT_CAMERA_MISMATCH
} AvpGpuNativeLayoutResult;

/* Source data: the exact 8-byte maze cells captured by Sweep 20, plus zero
 * or more caller-provided original-format resources (textures, object data,
 * overlays, pointer tables, etc.). The maze's retail DRAM address is NOT yet
 * established and must be supplied explicitly, not inferred.
 *
 * All hashes, addresses, regions and aliases are checked BEFORE the first
 * write. On any validation failure the ENTIRE destination is unchanged.
 * The camera at $7FB0 must already be staged via the S20 capture; it must
 * match the provided scene snapshot and its bytes cannot be overwritten. No allocations; no original game state mutations.
 * Neither a passing hash nor a mapped resource proves retail frame parity. */
AvpGpuNativeLayoutResult avp_gpu_native_stage_scene_resources(
    uint8_t *dram, size_t dram_size,
    const AvpGpuNativeLiveScene *scene,
    const uint8_t *maze_copy, size_t maze_capacity,
    uint32_t maze_address, uint64_t expected_maze_hash,
    const AvpGpuNativeSceneResource *resources, size_t resource_count);
#ifdef __cplusplus
}
#endif
#endif
