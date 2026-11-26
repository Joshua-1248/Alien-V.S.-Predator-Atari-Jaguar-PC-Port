#include "pc/pc_hybrid_pipeline_cache.h"
#include <stdlib.h>
#include <string.h>
/* Individual shader files are independently bounded by the SPIR-V reader;
 * retain a finite defensive limit even for callers outside that reader. */
#define AVP_HYBRID_SHADER_MAX_BYTES (16u * 1024u * 1024u)

int avp_hybrid_pipeline_key_valid(const AvpHybridPipelineKey *k)
{
    if(!k||!k->target_width||!k->target_height||!k->region_width||
       !k->region_height||!k->target_format||k->push_bytes<16u||
       k->push_bytes>128u||(k->push_bytes&3u)||
       k->region_x>=k->target_width||k->region_y>=k->target_height||
       k->region_width>k->target_width-k->region_x||
       k->region_height>k->target_height-k->region_y)return 0;
    const void *buffers[3]={k->vertex,k->world,k->foreground};
    const size_t lengths[3]={k->vertex_bytes,k->world_bytes,k->foreground_bytes};
    for(unsigned i=0;i<3;i++)
        if(!buffers[i]||lengths[i]<20u||lengths[i]>AVP_HYBRID_SHADER_MAX_BYTES||
           (lengths[i]&3u))return 0;
    return 1;
}

void avp_hybrid_pipeline_cache_invalidate(AvpHybridPipelineCache *cache)
{
    if(!cache)return;
    for(unsigned i=0;i<3;i++)free(cache->shader_copies[i]);
    memset(cache,0,sizeof(*cache));
}

static int metadata_equal(const AvpHybridPipelineKey *a,
                          const AvpHybridPipelineKey *b)
{
    return a->target_width==b->target_width&&
        a->target_height==b->target_height&&
        a->target_format==b->target_format&&
        a->region_x==b->region_x&&a->region_y==b->region_y&&
        a->region_width==b->region_width&&
        a->region_height==b->region_height&&
        a->push_bytes==b->push_bytes&&
        a->vertex_bytes==b->vertex_bytes&&
        a->world_bytes==b->world_bytes&&
        a->foreground_bytes==b->foreground_bytes;
}

int avp_hybrid_pipeline_cache_matches(const AvpHybridPipelineCache *cache,
                                      const AvpHybridPipelineKey *key)
{
    if(!cache||!cache->valid||!avp_hybrid_pipeline_key_valid(key)||
       !metadata_equal(&cache->key,key))return 0;
    const void *buffers[3]={key->vertex,key->world,key->foreground};
    const size_t lengths[3]={key->vertex_bytes,key->world_bytes,key->foreground_bytes};
    for(unsigned i=0;i<3;i++)
        if(!cache->shader_copies[i]||memcmp(cache->shader_copies[i],buffers[i],lengths[i]))
            return 0;
    return 1;
}

int avp_hybrid_pipeline_cache_capture(AvpHybridPipelineCache *cache,
                                      const AvpHybridPipelineKey *key)
{
    AvpHybridPipelineCache copy={0};
    const void *buffers[3];
    const size_t lengths[3]={key?key->vertex_bytes:0u,
       key?key->world_bytes:0u,key?key->foreground_bytes:0u};
    if(!cache||!avp_hybrid_pipeline_key_valid(key))return 0;
    buffers[0]=key->vertex;buffers[1]=key->world;buffers[2]=key->foreground;
    for(unsigned i=0;i<3;i++){
        copy.shader_copies[i]=malloc(lengths[i]);
        if(!copy.shader_copies[i]){
            avp_hybrid_pipeline_cache_invalidate(&copy);
            return 0;
        }
        memcpy(copy.shader_copies[i],buffers[i],lengths[i]);
    }
    copy.key=*key;
    copy.key.vertex=copy.shader_copies[0];
    copy.key.world=copy.shader_copies[1];
    copy.key.foreground=copy.shader_copies[2];
    copy.valid=1;
    avp_hybrid_pipeline_cache_invalidate(cache);
    *cache=copy;
    return 1;
}
