#include "jaguar/avp_gpu/original_view_layout.h"
#include <stdint.h>
#include <string.h>

/* Half-open host pointer intervals: avoid UB from unrelated-pointer ordering. */
static int disjoint_host(const void *p,size_t pn,const void *q,size_t qn)
{
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
    if(pn>UINTPTR_MAX-a||qn>UINTPTR_MAX-b)return 0;
    return a+pn<=b||b+qn<=a;
}
static int disjoint_dram(uint32_t a,size_t an,uint32_t b,size_t bn)
{
    return (size_t)a+an<=(size_t)b||(size_t)b+bn<=(size_t)a;
}
static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
static int fits(uint32_t a,size_t n)
{
    return n>0&&a<AVP_GPU_NATIVE_DRAM_SIZE&&
           n<=(size_t)AVP_GPU_NATIVE_DRAM_SIZE-a;
}
AvpGpuNativeLayoutResult avp_gpu_native_stage_scene_resources(
    uint8_t *dram, size_t dram_size,
    const AvpGpuNativeLiveScene *scene,
    const uint8_t *maze_copy, size_t maze_capacity,
    uint32_t maze_address, uint64_t expected_maze_hash,
    const AvpGpuNativeSceneResource *resources, size_t resource_count)
{
    size_t maze_bytes;
    if(!dram||!scene||!maze_copy||(resource_count&&!resources)||
       resource_count>AVP_GPU_NATIVE_MAX_SCENE_RESOURCES||
       !scene->maze_width||!scene->maze_height||
       scene->maze_width>AVP_GPU_NATIVE_MAX_MAZE_DIM||
       scene->maze_height>AVP_GPU_NATIVE_MAX_MAZE_DIM||
       !expected_maze_hash)return AVP_GPU_NATIVE_LAYOUT_INVALID;
    if(dram_size!=AVP_GPU_NATIVE_DRAM_SIZE)return AVP_GPU_NATIVE_LAYOUT_CAPACITY;
    maze_bytes=(size_t)scene->maze_width*scene->maze_height*AVP_GPU_NATIVE_MAZE_CELL_BYTES;
    if(maze_bytes!=scene->maze_bytes||maze_bytes>maze_capacity||
       !fits(maze_address,maze_bytes))return AVP_GPU_NATIVE_LAYOUT_CAPACITY;
    if(read_be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM)!=scene->camera_x||
       read_be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM+4u)!=scene->camera_y||
       read_be32(dram+AVP_GPU_NATIVE_CAMERA_DRAM+8u)!=scene->camera_angle)
        return AVP_GPU_NATIVE_LAYOUT_CAMERA_MISMATCH;
    if(!disjoint_host(dram,dram_size,scene,sizeof *scene)||
       !disjoint_host(dram,dram_size,maze_copy,maze_bytes)||
       (resource_count&&!disjoint_host(dram,dram_size,resources,
                             resource_count*sizeof *resources)))return AVP_GPU_NATIVE_LAYOUT_ALIAS;
    if(!disjoint_dram(maze_address,maze_bytes,AVP_GPU_NATIVE_CAMERA_DRAM,
                      AVP_GPU_NATIVE_CAMERA_BYTES))return AVP_GPU_NATIVE_LAYOUT_CONFLICT;
    if(avp_gpu_native_hash_bytes(maze_copy,maze_bytes)!=expected_maze_hash)
        return AVP_GPU_NATIVE_LAYOUT_HASH_MISMATCH;
    for(size_t i=0;i<resource_count;i++){
        const AvpGpuNativeSceneResource *r=&resources[i];
        if(!r->bytes||!r->expected_hash)return AVP_GPU_NATIVE_LAYOUT_INVALID;
        if(!fits(r->dram_address,r->length))return AVP_GPU_NATIVE_LAYOUT_CAPACITY;
        if(!disjoint_host(dram,dram_size,r->bytes,r->length))return AVP_GPU_NATIVE_LAYOUT_ALIAS;
        if(!disjoint_dram(r->dram_address,r->length,AVP_GPU_NATIVE_CAMERA_DRAM,
                          AVP_GPU_NATIVE_CAMERA_BYTES)||
           !disjoint_dram(r->dram_address,r->length,maze_address,maze_bytes))
            return AVP_GPU_NATIVE_LAYOUT_CONFLICT;
        for(size_t j=0;j<i;j++){
            const AvpGpuNativeSceneResource *s=&resources[j];
            if(!disjoint_dram(r->dram_address,r->length,s->dram_address,s->length))
                return AVP_GPU_NATIVE_LAYOUT_CONFLICT;
        }
        if(avp_gpu_native_hash_bytes(r->bytes,r->length)!=r->expected_hash)
            return AVP_GPU_NATIVE_LAYOUT_HASH_MISMATCH;
    }
    /* No validation below: every output memcpy is now in a known good range. */
    memcpy(dram+maze_address,maze_copy,maze_bytes);
    for(size_t i=0;i<resource_count;i++)
        memcpy(dram+resources[i].dram_address,resources[i].bytes,resources[i].length);
    return AVP_GPU_NATIVE_LAYOUT_OK;
}
