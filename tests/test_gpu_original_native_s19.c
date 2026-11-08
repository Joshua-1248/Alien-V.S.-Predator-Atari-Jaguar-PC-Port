/* ROM-free integration test for the host-independent native-C GPU stage driver.
 * With GEN_C, the stage body is real output of gpu_recomp.emit (JRISC->C).
 * Without GEN_C, a minimal stub tests the driver itself for the full suite. */
#include "jaguar/avp_gpu/original_view_native.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BANK (g->bank)
#ifdef GEN_C
#include GEN_C
#else
static uint32_t avp_gpu_stage0(AvpGpuRecompCtx *g,uint32_t entry)
{
    (void)entry;
    g->wr(g->ctx,0x400u,g->param[0],4);
    return 0xF03118u;
}
#endif

typedef struct Fixture { uint8_t *ram;uint8_t gpu[0x220];unsigned writes; } Fixture;
static uint32_t rd(void *ctx,uint32_t a,int width)
{
    Fixture *f=ctx;const uint8_t *p=NULL;uint32_t v=0u;
    if(a>=0xF03000u&&a<0xF03220u&&((uint32_t)width)<=0xF03220u-a)
        p=f->gpu+(a-0xF03000u);
    else if(a<0x200000u&&((uint32_t)width)<=0x200000u-a)
        p=f->ram+a;
    if(!p)return 0u;
    for(int i=0;i<width;i++)v=(v<<8)|p[i];
    return v;
}
static void wr(void *ctx,uint32_t a,uint32_t v,int width)
{
    Fixture *f=ctx;assert(a<0x200000u&&((uint32_t)width)<=0x200000u-a);
    f->writes++;
    for(int i=width-1;i>=0;i--){f->ram[a+i]=(uint8_t)v;v>>=8;}
}
static void w16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t *p,uint32_t v){w16(p,(uint16_t)(v>>16));w16(p+2,(uint16_t)v);}
static void movei(uint8_t *p,unsigned r,uint32_t v)
{
    w16(p,(uint16_t)((38u<<10)|r));
    w16(p+2,(uint16_t)v);
    w16(p+4,(uint16_t)(v>>16));
}
static void init(Fixture *f,uint32_t val)
{
    memset(f,0,sizeof(*f));f->ram=calloc(1,0x200000u);assert(f->ram);
    /* Synthetic resident: jump to overlay $F03220, unconditional. */
    movei(f->gpu,20u,0xF03220u);
    w16(f->gpu+6,(uint16_t)(52u<<10|20u<<5));
    w16(f->gpu+8,(uint16_t)(57u<<10));
    w32(f->ram+0x7F64,0x1000);
    /* Overlay: param MOVEI, store to DRAM[$400], jump to view halt. */
    movei(f->ram+0x1000,1u,val);
    movei(f->ram+0x1006,14u,0x400u);
    w16(f->ram+0x100C,(uint16_t)(47u<<10|14u<<5|1u));
    movei(f->ram+0x100E,20u,0xF03118u);
    w16(f->ram+0x1014,(uint16_t)(52u<<10|20u<<5));
    w16(f->ram+0x1016,(uint16_t)(57u<<10));
    w32(f->ram+0x280,val);
}
static uint64_t imagehash(Fixture *f)
{
    uint8_t b[24];memcpy(b,f->ram+0x1000,sizeof b);
    memset(b+2,0,4);return avp_gpu_native_hash_bytes(b,sizeof b);
}
static void setvalue(Fixture *f,uint32_t val)
{
    movei(f->ram+0x1000,1u,val);
    w32(f->ram+0x280,val);
}
static AvpGpuNativeResult run(AvpGpuNativePipeline *p,Fixture *f,uint32_t *end,unsigned *count)
{
    AvpGpuRecompCtx ctx;
    memset(&ctx,0,sizeof ctx);
    ctx.ctx=f;ctx.rd=rd;ctx.wr=wr;
    return avp_gpu_native_run(p,&ctx,end,count);
}
int main(void)
{
    Fixture f;AvpGpuNativeStage stage;
    AvpGpuNativeParam param={0u,0xF03220u,0x280u};
    AvpGpuNativePipeline p;uint32_t end=0u;unsigned calls=0;
    init(&f,0x12345678u);
    stage.entry=0xF03000u;stage.table=0x7F64u;stage.src=0x1000u;
    stage.longs=6u;stage.fn=avp_gpu_stage0;stage.image_hash=imagehash(&f);
    p.stages=&stage;p.stage_count=1u;p.params=&param;p.param_count=1u;
    p.resident_hash=avp_gpu_native_hash_bytes(f.gpu,sizeof f.gpu);
    p.entry=0xF03000u;p.halt=0xF03118u;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_OK);
    assert(end==p.halt&&calls==1u&&rd(&f,0x400u,4)==0x12345678u);
    setvalue(&f,0xBEEFF00Du); /* SAME generated C, different verified level operand */
    assert(stage.image_hash==imagehash(&f));
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_OK);
    assert(rd(&f,0x400u,4)==0xBEEFF00Du&&calls==1u);
    f.ram[0x1000]^=4u;f.writes=0;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_PATCH_MISMATCH);
    assert(f.writes==0u);f.ram[0x1000]^=4u;
    f.ram[0x1012]^=1u;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_IMAGE_MISMATCH);
    assert(f.writes==0u);f.ram[0x1012]^=1u;
    w32(f.ram+0x280,0x77777777u);
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_PATCH_MISMATCH);
    assert(f.writes==0u);w32(f.ram+0x280,0xBEEFF00Du);
    f.gpu[17]^=1u;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_IMAGE_MISMATCH);
    assert(f.writes==0u);f.gpu[17]^=1u;
    w32(f.ram+0x7F64,0x1010u);
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_IMAGE_MISMATCH);
    assert(f.writes==0u);w32(f.ram+0x7F64,0x1000u);
    param.gpu_pc=0xF03230u;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_BAD_CONFIG ||
           run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_PATCH_MISMATCH);
    assert(f.writes==0u);param.gpu_pc=0xF03220u;
    p.halt=0xF03080u;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_BAD_CONTROL_FLOW);
    p.halt=0xF03118u;
    p.stage_count=0u;
    assert(run(&p,&f,&end,&calls)==AVP_GPU_NATIVE_BAD_CONFIG);
    free(f.ram);
    puts("RE24 S19 native GPU stage driver and dynamic operand guards PASS");
    return 0;
}
