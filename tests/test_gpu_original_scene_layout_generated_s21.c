#include "jaguar/avp_gpu/original_view_layout.h"
#include "jaguar/avp_gpu/original_view_native.h"
#include "collision.h"
#include "maze.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BANK (g->bank)
#include GEN_C
u32 gpu_xpos,gpu_ypos,gpu_angle;
u16 maze_width,maze_height;
typedef struct Fixture {uint8_t *dram;uint8_t resident[0x220];unsigned writes;} Fixture;
static uint32_t rd(void *ctx,uint32_t addr,int w)
{
    Fixture *f=ctx;const uint8_t *p=NULL;uint32_t v=0;
    if(w<1||w>4)return 0;
    if(addr>=0xF03000u&&addr<=0xF03220u-(uint32_t)w)p=f->resident+addr-0xF03000u;
    else if(addr<=AVP_GPU_NATIVE_DRAM_SIZE-(uint32_t)w)p=f->dram+addr;
    if(!p)return 0;
    for(int i=0;i<w;i++)v=(v<<8)|p[i];
    return v;
}
static void wr(void *ctx,uint32_t addr,uint32_t v,int w)
{
    Fixture *f=ctx;assert(w>=1&&w<=4&&addr<=AVP_GPU_NATIVE_DRAM_SIZE-(uint32_t)w);
    f->writes++;
    for(int i=w-1;i>=0;i--){f->dram[addr+i]=(uint8_t)v;v>>=8;}
}
static int load(uint8_t *dst,size_t cap,const char *path)
{
    FILE *f=fopen(path,"rb");size_t n;
    if(!f)return 0;
    n=fread(dst,1,cap,f);
    if(fgetc(f)!=EOF){fclose(f);return 0;}
    fclose(f);return n==cap;
}
int main(int argc,char **argv)
{
    Fixture f={0};uint8_t maze[48],copied[48],texture[64],objects[16],overlay[OVERLAY_BYTES],table[4];
    AvpGpuNativeLiveScene scene;
    AvpGpuNativeSceneResource resources[4];
    AvpGpuNativeStage stage;
    AvpGpuNativePipeline pipeline;
    AvpGpuRecompCtx g={0};uint32_t pc=0;unsigned calls=0;
    assert(argc==3);
    assert(load(overlay,sizeof overlay,argv[1]));
    assert(load(f.resident,sizeof f.resident,argv[2]));
    f.dram=calloc(1,AVP_GPU_NATIVE_DRAM_SIZE);assert(f.dram);
    memset(maze,0,sizeof maze);memset(texture,0,sizeof texture);memset(objects,0,sizeof objects);
    maze[0]=0xA1;maze[1]=0xB2;maze[2]=0xC3;maze[3]=0xD4;
    texture[0]=0x11;texture[1]=0x22;texture[2]=0x33;texture[3]=0x44;
    objects[0]=0x55;objects[1]=0x66;objects[2]=0x77;objects[3]=0x88;
    table[0]=0;table[1]=1;table[2]=0x10;table[3]=0;
    avp_collision_bind_maze(maze,2,3);maze_width=2;maze_height=3;
    gpu_xpos=0x00188000u;gpu_ypos=0x00135000u;gpu_angle=0xC0000000u;
    assert(avp_gpu_native_capture_live_scene(f.dram,AVP_GPU_NATIVE_DRAM_SIZE,copied,sizeof copied,&scene)==AVP_GPU_NATIVE_SCENE_OK);
    resources[0]=(AvpGpuNativeSceneResource){0x4000u,texture,sizeof texture,avp_gpu_native_hash_bytes(texture,sizeof texture)};
    resources[1]=(AvpGpuNativeSceneResource){0x6000u,objects,sizeof objects,avp_gpu_native_hash_bytes(objects,sizeof objects)};
    resources[2]=(AvpGpuNativeSceneResource){0x11000u,overlay,sizeof overlay,avp_gpu_native_hash_bytes(overlay,sizeof overlay)};
    resources[3]=(AvpGpuNativeSceneResource){0x7F64u,table,sizeof table,avp_gpu_native_hash_bytes(table,sizeof table)};
    assert(avp_gpu_native_stage_scene_resources(f.dram,AVP_GPU_NATIVE_DRAM_SIZE,&scene,
        copied,sizeof copied,0x1000u,avp_gpu_native_hash_bytes(copied,sizeof copied),resources,4)==AVP_GPU_NATIVE_LAYOUT_OK);
    stage=(AvpGpuNativeStage){0xF03000u,0x7F64u,0x11000u,(uint32_t)(sizeof overlay/4u),
            avp_gpu_native_hash_bytes(overlay,sizeof overlay),avp_gpu_s21_stage};
    pipeline=(AvpGpuNativePipeline){&stage,1,NULL,0,
            avp_gpu_native_hash_bytes(f.resident,sizeof f.resident),0xF03000u,0xF03118u};
    g.ctx=&f;g.rd=rd;g.wr=wr;
    assert(avp_gpu_native_run(&pipeline,&g,&pc,&calls)==AVP_GPU_NATIVE_OK);
    assert(pc==0xF03118u&&calls==1u&&f.writes==4u);
    assert(rd(&f,0x500u,4)==0xA1B2C3D4u);
    assert(rd(&f,0x504u,4)==0x11223344u);
    assert(rd(&f,0x508u,4)==0x55667788u);
    assert(rd(&f,0x50Cu,4)==gpu_xpos);
    /* The resource loader does not bypass the existing stage image check. */
    f.dram[0x11002u]^=1u;f.writes=0;
    assert(avp_gpu_native_run(&pipeline,&g,&pc,&calls)==AVP_GPU_NATIVE_IMAGE_MISMATCH);
    assert(f.writes==0u);
    free(f.dram);
    puts("RE24 S21 genuine generated JRISC reads staged maze/texture/objects/camera PASS");
    return 0;
}
