#ifndef AVP_JAGUAR_ORIGINAL_VIEW_SCENE_H
#define AVP_JAGUAR_ORIGINAL_VIEW_SCENE_H
/* RE24 S20: exact 68000-owned camera and maze-cell capture for the native
 * original GPU view.  Only the camera's original DRAM address is established.
 * The maze cells are copied OUT of the live mutable game state into an
 * independent buffer, NOT placed at a guessed Jaguar DRAM address.
 * This code does not render, schedule the GPU, or alter the live game. */
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/original_view_native.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_GPU_NATIVE_CAMERA_DRAM 0x7FB0u
#define AVP_GPU_NATIVE_CAMERA_BYTES 12u
#define AVP_GPU_NATIVE_MAZE_CELL_BYTES 8u
#define AVP_GPU_NATIVE_MAX_MAZE_DIM 64u

typedef struct AvpGpuNativeLiveScene {
    uint32_t camera_x,camera_y,camera_angle; /* native MAZE NextFrame gpu_* snapshot */
    uint16_t maze_width,maze_height;
    size_t maze_bytes;
} AvpGpuNativeLiveScene;
typedef enum AvpGpuNativeSceneResult {
    AVP_GPU_NATIVE_SCENE_OK=0,
    AVP_GPU_NATIVE_SCENE_INVALID,
    AVP_GPU_NATIVE_SCENE_NOT_READY,
    AVP_GPU_NATIVE_SCENE_CAPACITY,
    AVP_GPU_NATIVE_SCENE_ALIAS
} AvpGpuNativeSceneResult;

/* Read 68000-owned host globals and the currently bound 8-byte/cell maze.
 * `dram` must be a separate full-size, disposable Jaguar memory image and
 * `maze_copy` must be separate nonaliasing storage.  On ANY error, neither
 * buffer nor `out` is modified.  The function performs no allocations.
 * Caller must invoke at a stable game-simulation snapshot, not concurrently
 * with an update to the maze or GPU pose. */
AvpGpuNativeSceneResult avp_gpu_native_capture_live_scene(
    uint8_t *dram,size_t dram_size,
    uint8_t *maze_copy,size_t maze_capacity,
    AvpGpuNativeLiveScene *out);

/* Decode the only mapped 3D-scene DRAM structure verified so far: $7FB0. */
int avp_gpu_native_read_camera(const uint8_t *dram,size_t dram_size,
                               uint32_t *x,uint32_t *y,uint32_t *angle);
#ifdef __cplusplus
}
#endif
#endif
