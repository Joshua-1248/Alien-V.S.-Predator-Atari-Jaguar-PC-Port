#include "pc/pc_hybrid_pipeline_cache.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"R363 pipeline cache check failed at line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
int main(void)
{
    uint32_t vs[8]={0x07230203u,0x10000u,1u,2u,3u,4u,5u,6u};
    uint32_t fs[8]={7u,8u,9u,10u,11u,12u,13u,14u};
    uint32_t fg[8]={15u,16u,17u,18u,19u,20u,21u,22u};
    AvpHybridPipelineKey k={1280u,720u,44u,0u,40u,1280u,640u,32u,
                           vs,fs,fg,sizeof(vs),sizeof(fs),sizeof(fg)};
    AvpHybridPipelineCache cache={0};
    uint32_t vs_copy[8];
    CHECK(avp_hybrid_pipeline_key_valid(&k));
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));
    CHECK(avp_hybrid_pipeline_cache_capture(&cache,&k));
    CHECK(avp_hybrid_pipeline_cache_matches(&cache,&k));
    memcpy(vs_copy,vs,sizeof(vs));k.vertex=vs_copy;
    CHECK(avp_hybrid_pipeline_cache_matches(&cache,&k));
    vs_copy[6]^=1u;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));
    vs_copy[6]^=1u;
    CHECK(avp_hybrid_pipeline_cache_matches(&cache,&k));
    fs[3]^=1u;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));fs[3]^=1u;
    fg[3]^=1u;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));fg[3]^=1u;
    k.region_y++;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));k.region_y--;
    k.target_format++;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));k.target_format--;
    k.region_width--;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));k.region_width++;
    k.push_bytes+=4u;
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));k.push_bytes-=4u;
    k.target_width--;
    CHECK(!avp_hybrid_pipeline_key_valid(&k));k.target_width++;
    k.vertex_bytes=31u;
    CHECK(!avp_hybrid_pipeline_key_valid(&k));k.vertex_bytes=sizeof(vs);
    CHECK(avp_hybrid_pipeline_cache_matches(&cache,&k));
    CHECK(avp_hybrid_pipeline_cache_capture(&cache,&k));
    CHECK(avp_hybrid_pipeline_cache_matches(&cache,&k));
    k.foreground_bytes=0u;
    CHECK(!avp_hybrid_pipeline_cache_capture(&cache,&k));
    k.foreground_bytes=sizeof(fg);
    CHECK(avp_hybrid_pipeline_cache_matches(&cache,&k));
    avp_hybrid_pipeline_cache_invalidate(&cache);
    CHECK(!avp_hybrid_pipeline_cache_matches(&cache,&k));
    CHECK(!cache.key.vertex&&!cache.key.world&&!cache.key.foreground);
    puts("R363 exact shader and target-region pipeline cache: PASS");
    return 0;
}
