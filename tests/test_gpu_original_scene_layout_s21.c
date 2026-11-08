/* S21: transactional ROM-capture-addressed scene staging; no retail bytes. */
#include "jaguar/avp_gpu/original_view_layout.h"
#include "jaguar/avp_gpu/original_view_native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t maze[48],texture[91],objects[25],overlay[60],table[20];
static void fill(uint8_t *p,size_t n,unsigned seed)
{ for(size_t i=0;i<n;i++)p[i]=(uint8_t)(seed+i*29u); }
static uint64_t hash(const uint8_t *p,size_t n)
{ return avp_gpu_native_hash_bytes(p,n); }
static void unchanged(uint8_t *dram,uint8_t *before)
{ assert(memcmp(dram,before,AVP_GPU_NATIVE_DRAM_SIZE)==0); }
int main(void)
{
    uint8_t *dram=malloc(AVP_GPU_NATIVE_DRAM_SIZE),*before=malloc(AVP_GPU_NATIVE_DRAM_SIZE);
    AvpGpuNativeLiveScene scene={0x00188000u,0x00135000u,0xC0000000u,2,3,sizeof maze};
    AvpGpuNativeSceneResource r[4];
    AvpGpuNativeLayoutResult v;
    assert(dram&&before);
    fill(maze,sizeof maze,2);fill(texture,sizeof texture,5);
    fill(objects,sizeof objects,9);fill(overlay,sizeof overlay,11);
    memset(table,0,sizeof table);
    table[0]=0;table[1]=1;table[2]=0x10;table[3]=0;
    r[0]=(AvpGpuNativeSceneResource){0x4000u,texture,sizeof texture,hash(texture,sizeof texture)};
    r[1]=(AvpGpuNativeSceneResource){0x6000u,objects,sizeof objects,hash(objects,sizeof objects)};
    r[2]=(AvpGpuNativeSceneResource){0x11000u,overlay,sizeof overlay,hash(overlay,sizeof overlay)};
    r[3]=(AvpGpuNativeSceneResource){0x7F64u,table,sizeof table,hash(table,sizeof table)};
    memset(dram,0x77,AVP_GPU_NATIVE_DRAM_SIZE);
    /* S20 camera write already performed; mapping must not alter it. */
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+0]=0x00;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+1]=0x18;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+2]=0x80;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+3]=0x00;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+4]=0x00;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+5]=0x13;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+6]=0x50;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+7]=0x00;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+8]=0xC0;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+9]=0x00;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+10]=0x00;
    dram[AVP_GPU_NATIVE_CAMERA_DRAM+11]=0x00;
    memcpy(before,dram,AVP_GPU_NATIVE_DRAM_SIZE);
#define STAGE(ADDR,HASH) avp_gpu_native_stage_scene_resources(dram,AVP_GPU_NATIVE_DRAM_SIZE,\
   &scene,maze,sizeof maze,(ADDR),(HASH),r,4)
#define FAILURE(EXPR,EXPECTED) do {v=(EXPR); assert(v==(EXPECTED)); unchanged(dram,before);} while(0)
    scene.camera_x^=1u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CAMERA_MISMATCH);
    scene.camera_x^=1u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)^1u),AVP_GPU_NATIVE_LAYOUT_HASH_MISMATCH);
    r[1].expected_hash^=1u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_HASH_MISMATCH);
    r[1].expected_hash^=1u;
    r[0].dram_address=0x6004u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CONFLICT);
    r[0].dram_address=0x4000u;
    r[0].dram_address=0x7FB1u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CONFLICT);
    r[0].dram_address=0x4000u;
    FAILURE(STAGE(0x7FB0u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CONFLICT);
    FAILURE(STAGE(AVP_GPU_NATIVE_DRAM_SIZE-20u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CAPACITY);
    r[0].dram_address=AVP_GPU_NATIVE_DRAM_SIZE-2u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CAPACITY);
    r[0].dram_address=0x4000u;
    r[0].bytes=dram+0x20000u;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_ALIAS);
    r[0].bytes=texture;
    FAILURE(avp_gpu_native_stage_scene_resources(dram,AVP_GPU_NATIVE_DRAM_SIZE,&scene,
             dram+0x8000u,sizeof maze,0x1000u,hash(maze,sizeof maze),r,4),AVP_GPU_NATIVE_LAYOUT_ALIAS);
    scene.maze_bytes--;
    FAILURE(STAGE(0x1000u,hash(maze,sizeof maze)),AVP_GPU_NATIVE_LAYOUT_CAPACITY);
    scene.maze_bytes++;
    FAILURE(avp_gpu_native_stage_scene_resources(dram,AVP_GPU_NATIVE_DRAM_SIZE,&scene,
             maze,sizeof maze,0x1000u,hash(maze,sizeof maze),r,65),AVP_GPU_NATIVE_LAYOUT_INVALID);
    FAILURE(avp_gpu_native_stage_scene_resources(dram,AVP_GPU_NATIVE_DRAM_SIZE-1,&scene,
             maze,sizeof maze,0x1000u,hash(maze,sizeof maze),r,4),AVP_GPU_NATIVE_LAYOUT_CAPACITY);
    /* A caller-supplied synthetic layout works, but does not establish a retail layout. */
    assert(STAGE(0x1000u,hash(maze,sizeof maze))==AVP_GPU_NATIVE_LAYOUT_OK);
    assert(!memcmp(dram+0x1000u,maze,sizeof maze));
    assert(!memcmp(dram+0x4000u,texture,sizeof texture));
    assert(!memcmp(dram+0x6000u,objects,sizeof objects));
    assert(!memcmp(dram+0x11000u,overlay,sizeof overlay));
    assert(!memcmp(dram+0x7F64u,table,sizeof table));
    assert(!memcmp(dram+AVP_GPU_NATIVE_CAMERA_DRAM,
                   before+AVP_GPU_NATIVE_CAMERA_DRAM,AVP_GPU_NATIVE_CAMERA_BYTES));
    assert(!memcmp(dram,before,0x1000u));
    free(before);free(dram);
    puts("RE24 S21 mapped scene preflight + no-write failures PASS");
    return 0;
}
