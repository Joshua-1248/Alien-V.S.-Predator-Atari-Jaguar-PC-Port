#include "jaguar/avp_gpu/original_view_native.h"
#include "jaguar/avp_gpu/original_view_scene.h"
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
typedef struct Fixture {uint8_t *dram;uint8_t resident[0x220];uint32_t writes;} Fixture;
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
    Fixture f={0};uint8_t m[8*4]={0},copied[sizeof m];
    AvpGpuNativeLiveScene meta;
    AvpGpuNativeStage st;
    AvpGpuNativePipeline pipeline;
    AvpGpuRecompCtx g={0};
    uint32_t pc=0;unsigned calls=0;
    uint8_t overlay[44];
    assert(argc==3);
    assert(load(overlay,sizeof overlay,argv[1]));
    assert(load(f.resident,sizeof f.resident,argv[2]));
    f.dram=calloc(1,AVP_GPU_NATIVE_DRAM_SIZE);assert(f.dram);
    for(unsigned i=0;i<sizeof m;i++)m[i]=(uint8_t)(i*31u+5u);
    avp_collision_bind_maze(m,2,2);maze_width=2;maze_height=2;
    memcpy(f.dram+0x1000u,overlay,sizeof overlay);
    f.dram[0x7F64]=0u;f.dram[0x7F65]=0u;f.dram[0x7F66]=0x10u;f.dram[0x7F67]=0u;
    st=(AvpGpuNativeStage){0xF03000u,0x7F64u,0x1000u,(uint32_t)sizeof overlay/4u,
                           avp_gpu_native_hash_bytes(overlay,sizeof overlay),avp_gpu_s20_stage};
    pipeline=(AvpGpuNativePipeline){&st,1,NULL,0,avp_gpu_native_hash_bytes(f.resident,sizeof f.resident),0xF03000u,0xF03118u};
    g.ctx=&f;g.rd=rd;g.wr=wr;
    for(unsigned i=0;i<2;i++){
        uint32_t x=(i==0)?0x00188000u:0xABCD3456u;
        uint32_t y=(i==0)?0x00135000u:0xDEADBEEFu;
        uint32_t a=(i==0)?0xC0000000u:0x40000000u;
        gpu_xpos=x;gpu_ypos=y;gpu_angle=a;
        assert(avp_gpu_native_capture_live_scene(f.dram,AVP_GPU_NATIVE_DRAM_SIZE,copied,sizeof copied,&meta)==AVP_GPU_NATIVE_SCENE_OK);
        assert(!memcmp(copied,m,sizeof m));
        f.writes=0;
        assert(avp_gpu_native_run(&pipeline,&g,&pc,&calls)==AVP_GPU_NATIVE_OK);
        assert(pc==pipeline.halt&&calls==1u&&f.writes==3u);
        assert(rd(&f,0x500u,4)==x&&rd(&f,0x504u,4)==y&&rd(&f,0x508u,4)==a);
    }
    /* Stale overlay must be rejected before the generated stage writes. */
    f.dram[0x1001]^=1u;f.writes=0;
    assert(avp_gpu_native_run(&pipeline,&g,&pc,&calls)==AVP_GPU_NATIVE_IMAGE_MISMATCH);
    assert(!f.writes);
    free(f.dram);
    puts("RE24 S20 genuine generated JRISC -> live scene $7FB0 PASS");
    return 0;
}
