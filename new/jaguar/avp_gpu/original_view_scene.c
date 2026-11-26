#include "jaguar/avp_gpu/original_view_scene.h"
#include "maze.h"
#include "collision.h"
#include <stdint.h>
#include <string.h>

static void be32(uint8_t *dst,uint32_t v)
{
    dst[0]=(uint8_t)(v>>24);dst[1]=(uint8_t)(v>>16);
    dst[2]=(uint8_t)(v>>8);dst[3]=(uint8_t)v;
}
static uint32_t from_be32(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
/* Overflow-safe half-open ranges, for rejecting aliasing without UB from
 * relational comparison of pointers to unrelated C objects. */
static int separate(const void *p,size_t pn,const void *q,size_t qn)
{
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
    if(pn>UINTPTR_MAX-a||qn>UINTPTR_MAX-b)return 0;
    return a+pn<=b||b+qn<=a;
}
AvpGpuNativeSceneResult avp_gpu_native_capture_live_scene(
    uint8_t *dram,size_t dram_size,
    uint8_t *maze_copy,size_t maze_capacity,
    AvpGpuNativeLiveScene *out)
{
    const uint8_t *source;
    uint16_t w,h;
    size_t n;
    AvpGpuNativeLiveScene state;
    if(!dram||!maze_copy||!out)return AVP_GPU_NATIVE_SCENE_INVALID;
    if(dram_size!=AVP_GPU_NATIVE_DRAM_SIZE)return AVP_GPU_NATIVE_SCENE_CAPACITY;
    source=avp_collision_maze_data();
    w=avp_collision_maze_width();h=avp_collision_maze_height();
    if(!source||!w||!h)return AVP_GPU_NATIVE_SCENE_NOT_READY;
    if(w>AVP_GPU_NATIVE_MAX_MAZE_DIM||h>AVP_GPU_NATIVE_MAX_MAZE_DIM||
       w!=maze_width||h!=maze_height)return AVP_GPU_NATIVE_SCENE_INVALID;
    n=(size_t)w*(size_t)h*AVP_GPU_NATIVE_MAZE_CELL_BYTES;
    if(maze_capacity<n)return AVP_GPU_NATIVE_SCENE_CAPACITY;
    if(!separate(dram,dram_size,maze_copy,n)||
       !separate(source,n,maze_copy,n)||
       !separate(source,n,dram,dram_size)||
       !separate(out,sizeof *out,dram,dram_size)||
       !separate(out,sizeof *out,maze_copy,n)||
       !separate(out,sizeof *out,source,n))
        return AVP_GPU_NATIVE_SCENE_ALIAS;
    /* The gpu_* snapshot is published by NextFrame before gameplay updates.
     * Do not use x_pos/y_pos/centre_angle, which may already be a new tick. */
    state.camera_x=gpu_xpos;
    state.camera_y=gpu_ypos;
    state.camera_angle=gpu_angle;
    state.maze_width=w;state.maze_height=h;state.maze_bytes=n;
    memcpy(maze_copy,source,n);
    be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM,state.camera_x);
    be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM+4u,state.camera_y);
    be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM+8u,state.camera_angle);
    *out=state;
    return AVP_GPU_NATIVE_SCENE_OK;
}
int avp_gpu_native_read_camera(const uint8_t *dram,size_t size,
                               uint32_t *x,uint32_t *y,uint32_t *angle)
{
    if(!dram||size<AVP_GPU_NATIVE_CAMERA_DRAM+AVP_GPU_NATIVE_CAMERA_BYTES||
       !x||!y||!angle)return 0;
    *x=from_be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM);
    *y=from_be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM+4u);
    *angle=from_be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM+8u);
    return 1;
}
