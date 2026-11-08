#include "jaguar/avp_gpu/original_view_native.h"
#include <limits.h>

#define FNV_OFFSET UINT64_C(14695981039346656037)
#define FNV_PRIME  UINT64_C(1099511628211)

uint64_t avp_gpu_native_hash_bytes(const uint8_t *bytes,size_t len)
{
    uint64_t h=FNV_OFFSET;
    if(!bytes&&len)return 0u;
    for(size_t i=0;i<len;i++){h^=bytes[i];h*=FNV_PRIME;}
    return h;
}
static uint32_t readbus(const AvpGpuRecompCtx *g,uint32_t addr,int width)
{ return g->rd(g->ctx,addr,width); }
static uint64_t hash_resident(const AvpGpuRecompCtx *g)
{
    uint64_t h=FNV_OFFSET;
    for(unsigned i=0;i<AVP_GPU_NATIVE_RESIDENT_SIZE;i++){
        h^=readbus(g,AVP_GPU_NATIVE_RESIDENT_BASE+i,1)&0xffu;
        h*=FNV_PRIME;
    }
    return h;
}
static int masked(const AvpGpuNativePipeline *p,unsigned stage,uint32_t off)
{
    for(size_t i=0;i<p->param_count;i++){
        const AvpGpuNativeParam *q=&p->params[i];
        if(q->stage!=stage)continue;
        uint32_t start=q->gpu_pc-AVP_GPU_NATIVE_OVERLAY_BASE+2u;
        if(off>=start&&off-start<4u)return 1;
    }
    return 0;
}
static uint64_t hash_overlay(const AvpGpuNativePipeline *p,const AvpGpuRecompCtx *g,unsigned stage)
{
    const AvpGpuNativeStage *s=&p->stages[stage];
    uint64_t h=FNV_OFFSET;uint32_t bytes=s->longs*4u;
    for(uint32_t off=0;off<bytes;off++){
        uint32_t b=masked(p,stage,off)?0u:readbus(g,s->src+off,1)&0xffu;
        h^=b;h*=FNV_PRIME;
    }
    return h;
}
static AvpGpuNativeResult validate(const AvpGpuNativePipeline *p,AvpGpuRecompCtx *g,
                                   unsigned stage,int setparams)
{
    const AvpGpuNativeStage *s=&p->stages[stage];
    if((readbus(g,s->table,4)&0x1fffffu)!=s->src)return AVP_GPU_NATIVE_IMAGE_MISMATCH;
    for(size_t j=0;j<p->param_count;j++){
        const AvpGpuNativeParam *q=&p->params[j];
        if(q->stage!=stage)continue;
        uint32_t off=q->gpu_pc-AVP_GPU_NATIVE_OVERLAY_BASE;
        uint32_t ins=readbus(g,s->src+off,2)&0xffffu;
        uint32_t imm=(readbus(g,s->src+off+2u,2)&0xffffu)|
            ((readbus(g,s->src+off+4u,2)&0xffffu)<<16);
        uint32_t var=readbus(g,q->variable,4);
        if((ins>>10)!=38u||var!=imm)return AVP_GPU_NATIVE_PATCH_MISMATCH;
        if(setparams)g->param[j]=var;
    }
    if(hash_overlay(p,g,stage)!=s->image_hash)return AVP_GPU_NATIVE_IMAGE_MISMATCH;
    return AVP_GPU_NATIVE_OK;
}
AvpGpuNativeResult avp_gpu_native_run(const AvpGpuNativePipeline *p,
                                      AvpGpuRecompCtx *g,
                                      uint32_t *end_pc,unsigned *stage_calls)
{
    uint32_t pc;unsigned calls=0;
    if(end_pc)*end_pc=0u;
    if(stage_calls)*stage_calls=0u;
    if(!p||!g||!g->rd||!g->wr||!p->stages||!p->stage_count||
       p->stage_count>64u||p->param_count>AVP_GPU_NATIVE_MAX_PARAMS||
       (p->param_count&&!p->params)||!p->resident_hash||
       p->entry!=AVP_GPU_NATIVE_RESIDENT_BASE||p->halt==p->entry||
       g->bank>1u)return AVP_GPU_NATIVE_BAD_CONFIG;
    for(size_t i=0;i<p->stage_count;i++){
        const AvpGpuNativeStage *s=&p->stages[i];
        if(!s->fn||!s->image_hash||s->entry<AVP_GPU_NATIVE_RESIDENT_BASE||
           s->entry>=AVP_GPU_NATIVE_OVERLAY_BASE||
           s->table>AVP_GPU_NATIVE_DRAM_SIZE-4u||
           s->src>=AVP_GPU_NATIVE_DRAM_SIZE||!s->longs||
           s->longs>(AVP_GPU_NATIVE_DRAM_SIZE-s->src)/4u||
           s->longs>1024u||s->src%4u)return AVP_GPU_NATIVE_BAD_CONFIG;
        for(size_t k=0;k<i;k++)if(p->stages[k].entry==s->entry)return AVP_GPU_NATIVE_BAD_CONFIG;
    }
    if(p->stages[0].entry!=p->entry)return AVP_GPU_NATIVE_BAD_CONFIG;
    for(size_t j=0;j<p->param_count;j++){
        const AvpGpuNativeParam *q=&p->params[j];
        if(q->stage>=p->stage_count||q->gpu_pc<AVP_GPU_NATIVE_OVERLAY_BASE||
           q->variable>AVP_GPU_NATIVE_DRAM_SIZE-4u||q->variable%2u)return AVP_GPU_NATIVE_BAD_CONFIG;
        const AvpGpuNativeStage *s=&p->stages[q->stage];
        uint32_t off=q->gpu_pc-AVP_GPU_NATIVE_OVERLAY_BASE;
        if(s->longs*4u<6u||off>s->longs*4u-6u||off%2u)return AVP_GPU_NATIVE_BAD_CONFIG;
        for(size_t k=0;k<j;k++)if(p->params[k].stage==q->stage&&
            p->params[k].gpu_pc==q->gpu_pc)return AVP_GPU_NATIVE_BAD_CONFIG;
    }
    if(hash_resident(g)!=p->resident_hash)return AVP_GPU_NATIVE_IMAGE_MISMATCH;
    /* Transactional preflight: no generated code runs if any overlay is stale. */
    for(size_t i=0;i<p->stage_count;i++){
        AvpGpuNativeResult v=validate(p,g,(unsigned)i,0);
        if(v!=AVP_GPU_NATIVE_OK)return v;
    }
    pc=p->entry;
    while(pc!=p->halt&&calls<p->stage_count){
        size_t i=0;AvpGpuNativeResult result;
        for(;i<p->stage_count&&p->stages[i].entry!=pc;i++){}
        if(i==p->stage_count)return AVP_GPU_NATIVE_BAD_CONTROL_FLOW;
        result=validate(p,g,(unsigned)i,1);
        if(result!=AVP_GPU_NATIVE_OK)return result;
        if(g->stop||g->fault)return AVP_GPU_NATIVE_EXECUTION_FAULT;
        pc=p->stages[i].fn(g,pc);
        calls++;
        if(g->fault||g->stop)return AVP_GPU_NATIVE_EXECUTION_FAULT;
    }
    if(end_pc)*end_pc=pc;
    if(stage_calls)*stage_calls=calls;
    return pc==p->halt?AVP_GPU_NATIVE_OK:AVP_GPU_NATIVE_BAD_CONTROL_FLOW;
}
