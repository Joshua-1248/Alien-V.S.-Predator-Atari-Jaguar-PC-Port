/* RE24 S20: actual collision maze binding + 68000 GPU snapshot -> Jaguar RAM.
 * ROM-free, original maze cell payload untouched, no live render changes. */
#include "jaguar/avp_gpu/original_view_scene.h"
#include "collision.h"
#include "maze.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Only the state used by the isolated live-scene bridge is linked.  The real
 * collision.c avp_collision_bind_maze/accessors are exercised as-is. */
u32 gpu_xpos,gpu_ypos,gpu_angle;
u16 maze_width,maze_height;
static uint8_t original[2*3*8];
static uint32_t x,y,angle;
static void assert_camera(const uint8_t *dram,uint32_t ex,uint32_t ey,uint32_t ea)
{
    assert(avp_gpu_native_read_camera(dram,AVP_GPU_NATIVE_DRAM_SIZE,&x,&y,&angle));
    assert(x==ex&&y==ey&&angle==ea);
}
int main(void)
{
    const size_t length=AVP_GPU_NATIVE_DRAM_SIZE;
    uint8_t *dram=malloc(length),*before=malloc(length),cells[48],backup[48];
    AvpGpuNativeLiveScene meta,old;
    assert(dram&&before);
    for(unsigned i=0;i<sizeof original;i++)original[i]=(uint8_t)(i*17u+3u);
    memset(dram,0x5a,length);memset(cells,0xa5,sizeof cells);
    memcpy(before,dram,length);memcpy(backup,cells,sizeof cells);
    memset(&meta,0xcc,sizeof meta);old=meta;
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells,&meta)==AVP_GPU_NATIVE_SCENE_NOT_READY);
    assert(!memcmp(dram,before,length)&&!memcmp(cells,backup,sizeof cells)&&!memcmp(&meta,&old,sizeof meta));
    avp_collision_bind_maze(original,2,3);maze_width=2;maze_height=3;
    gpu_xpos=0x00188000u;gpu_ypos=0x00135000u;gpu_angle=0xC0000000u;
    assert(avp_gpu_native_capture_live_scene(dram,length-1,cells,sizeof cells,&meta)==AVP_GPU_NATIVE_SCENE_CAPACITY);
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells-1,&meta)==AVP_GPU_NATIVE_SCENE_CAPACITY);
    assert(avp_gpu_native_capture_live_scene(dram,length,dram+0x20000u,length-0x20000u,&meta)==AVP_GPU_NATIVE_SCENE_ALIAS);
    assert(avp_gpu_native_capture_live_scene(dram,length,original,sizeof original,&meta)==AVP_GPU_NATIVE_SCENE_ALIAS);
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells,(AvpGpuNativeLiveScene *)(void *)(dram+0x300u))==AVP_GPU_NATIVE_SCENE_ALIAS);
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells,(AvpGpuNativeLiveScene *)(void *)original)==AVP_GPU_NATIVE_SCENE_ALIAS);
    maze_height=2;
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells,&meta)==AVP_GPU_NATIVE_SCENE_INVALID);
    maze_height=3;
    assert(!memcmp(dram,before,length)&&!memcmp(cells,backup,sizeof cells)&&!memcmp(&meta,&old,sizeof meta));
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells,&meta)==AVP_GPU_NATIVE_SCENE_OK);
    assert(meta.camera_x==gpu_xpos&&meta.camera_y==gpu_ypos&&meta.camera_angle==gpu_angle);
    assert(meta.maze_width==2u&&meta.maze_height==3u&&meta.maze_bytes==sizeof original);
    assert(!memcmp(cells,original,sizeof original));
    assert_camera(dram,0x00188000u,0x00135000u,0xC0000000u);
    assert(!memcmp(dram,before,AVP_GPU_NATIVE_CAMERA_DRAM));
    assert(!memcmp(dram+AVP_GPU_NATIVE_CAMERA_DRAM+AVP_GPU_NATIVE_CAMERA_BYTES,
                   before+AVP_GPU_NATIVE_CAMERA_DRAM+AVP_GPU_NATIVE_CAMERA_BYTES,
                   length-AVP_GPU_NATIVE_CAMERA_DRAM-AVP_GPU_NATIVE_CAMERA_BYTES));
    /* Change gameplay position without advancing the GPU snapshot: the bridge
     * must still use gpu_* (the original NextFrame order), not the new pose. */
    gpu_xpos=0x12345678u;gpu_ypos=0xfedcba98u;gpu_angle=0x70000000u;
    original[1]=0xf9u;
    assert(avp_gpu_native_capture_live_scene(dram,length,cells,sizeof cells,&meta)==AVP_GPU_NATIVE_SCENE_OK);
    assert_camera(dram,0x12345678u,0xfedcba98u,0x70000000u);
    assert(cells[1]==0xf9u);
    assert(!avp_gpu_native_read_camera(dram,AVP_GPU_NATIVE_CAMERA_DRAM+11u,&x,&y,&angle));
    free(before);free(dram);
    puts("RE24 S20 live GPU camera + raw 8-byte maze snapshot PASS");
    return 0;
}
