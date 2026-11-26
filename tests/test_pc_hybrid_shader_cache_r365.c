#define _POSIX_C_SOURCE 200809L
/* R365 transactional, SDK-free shader cache test. These streams test the
 * structural gate only, not a real GPU SPIR-V module or driver. */
#include "pc/pc_hybrid_shader_cache.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"R365 shader cache test failed at %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int write_stage(const char *path,uint32_t model,uint32_t generation,int bad)
{
    char alternate[1024];
    static unsigned ordinal=0u;
    uint32_t x[]={0x07230203u,0x00010000u,0u,32u,0u,
                  (2u<<16)|17u,1u,(3u<<16)|14u,0u,1u,
                  (5u<<16)|15u,model,4u,0x6e69616du,0u};
    FILE *f;
    if(snprintf(alternate,sizeof(alternate),"%s.tmp%u",path,ordinal++)>=
        (int)sizeof(alternate))return 0;
    x[2]=generation;
    if(bad)x[0]=0u;
    f=fopen(alternate,"wb");
    if(!f)return 0;
    if(fwrite(x,sizeof(x),1u,f)!=1u||fclose(f)!=0){unlink(alternate);return 0;}
    if(rename(alternate,path)!=0){unlink(alternate);return 0;}
    return 1;
}
int main(void)
{
    char dir[]="/tmp/avp_r365_shaders_XXXXXX";
    char paths[3][1024];
    AvpHybridShaderCache cache={0};
    uint32_t *prev[3]={NULL};
    CHECK(mkdtemp(dir)!=NULL);
    for(unsigned i=0;i<3u;i++){
        CHECK(snprintf(paths[i],sizeof(paths[i]),"%s/%u.spv",dir,i)<
              (int)sizeof(paths[i]));
        CHECK(write_stage(paths[i],i==0u?0u:4u,i+1u,0));
    }
    CHECK(!avp_hybrid_shader_cache_refresh(NULL,paths[0],paths[1],paths[2]));
    CHECK(avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
    CHECK(cache.updates==1u);
    for(unsigned i=0;i<3u;i++){
        CHECK(cache.entries[i].words && cache.entries[i].bytes==60u);
        prev[i]=cache.entries[i].words;
    }
    for(unsigned i=0;i<15u;i++){
        CHECK(avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
        CHECK(cache.updates==1u);
        for(unsigned j=0;j<3u;j++)CHECK(cache.entries[j].words==prev[j]);
    }
    CHECK(write_stage(paths[1],4u,50u,0));
    CHECK(avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
    CHECK(cache.updates==2u&&cache.entries[0].words==prev[0]&&
          cache.entries[2].words==prev[2]&&cache.entries[1].words[2]==50u);
    prev[1]=cache.entries[1].words;
    CHECK(write_stage(paths[1],0u,51u,0)); /* wrong shader stage */
    CHECK(!avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
    CHECK(cache.updates==2u&&cache.entries[1].words==prev[1]);
    CHECK(write_stage(paths[1],4u,52u,0));
    CHECK(write_stage(paths[2],4u,53u,1)); /* valid change then bad last */
    CHECK(!avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
    CHECK(cache.updates==2u&&cache.entries[1].words==prev[1]&&
          cache.entries[2].words==prev[2]); /* transactional rollback */
    CHECK(write_stage(paths[2],4u,54u,0));
    CHECK(avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
    CHECK(cache.updates==3u&&cache.entries[1].words[2]==52u&&
          cache.entries[2].words[2]==54u);
    CHECK(unlink(paths[0])==0);
    CHECK(!avp_hybrid_shader_cache_refresh(&cache,paths[0],paths[1],paths[2]));
    CHECK(cache.updates==3u);
    avp_hybrid_shader_cache_dispose(&cache);
    for(unsigned i=0;i<3u;i++){if(i)unlink(paths[i]);}
    rmdir(dir);
    puts("R365 cached SPIR-V stage bytes, unchanged-frame reuse and atomic reload PASS");
    return 0;
}
