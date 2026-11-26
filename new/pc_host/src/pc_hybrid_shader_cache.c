#define _POSIX_C_SOURCE 200809L
#include "pc/pc_hybrid_shader_cache.h"
#include "pc/pc_vulkan_spirv_contract.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

static int stamp_file(const char *path, AvpHybridShaderStamp *s)
{
    struct stat st;
    if(!path||!*path||!s||stat(path,&st)!=0||!S_ISREG(st.st_mode)||
       st.st_size<20||st.st_size>AVP_SPIRV_MAX_BYTES||
       (st.st_size&3)!=0)return 0;
    s->device=(uint64_t)st.st_dev;
    s->inode=(uint64_t)st.st_ino;
    s->bytes=(uint64_t)st.st_size;
    s->mtime_seconds=(int64_t)st.st_mtim.tv_sec;
    s->mtime_nanos=(int64_t)st.st_mtim.tv_nsec;
    s->ctime_seconds=(int64_t)st.st_ctim.tv_sec;
    s->ctime_nanos=(int64_t)st.st_ctim.tv_nsec;
    return 1;
}
static int same_stamp(const AvpHybridShaderStamp *a,
                      const AvpHybridShaderStamp *b)
{
    return a->device==b->device && a->inode==b->inode &&
        a->bytes==b->bytes && a->mtime_seconds==b->mtime_seconds &&
        a->mtime_nanos==b->mtime_nanos &&
        a->ctime_seconds==b->ctime_seconds && a->ctime_nanos==b->ctime_nanos;
}
void avp_hybrid_shader_cache_dispose(AvpHybridShaderCache *cache)
{
    if(!cache)return;
    for(unsigned i=0;i<AVP_HYBRID_SHADER_COUNT;i++)free(cache->entries[i].words);
    memset(cache,0,sizeof(*cache));
}
int avp_hybrid_shader_cache_refresh(AvpHybridShaderCache *cache,
    const char *vertex,const char *world,const char *foreground)
{
    const char *paths[AVP_HYBRID_SHADER_COUNT]={vertex,world,foreground};
    const uint32_t models[AVP_HYBRID_SHADER_COUNT]={AVP_SPIRV_VERTEX,
        AVP_SPIRV_FRAGMENT,AVP_SPIRV_FRAGMENT};
    AvpHybridShaderStamp stamps[AVP_HYBRID_SHADER_COUNT]={{0}};
    uint32_t *pending[AVP_HYBRID_SHADER_COUNT]={NULL};
    size_t sizes[AVP_HYBRID_SHADER_COUNT]={0};
    int changed[AVP_HYBRID_SHADER_COUNT]={0};
    int any=0;
    if(!cache)return 0;
    for(unsigned i=0;i<AVP_HYBRID_SHADER_COUNT;i++){
        size_t length;
        const AvpHybridShaderEntry *entry=&cache->entries[i];
        if(!paths[i])goto fail;
        length=strlen(paths[i]);
        if(!length||length>=AVP_HYBRID_SHADER_PATH_MAX||
           !stamp_file(paths[i],&stamps[i]))goto fail;
        if(entry->words && strcmp(entry->path,paths[i])==0 &&
           same_stamp(&entry->stamp,&stamps[i]))continue;
        changed[i]=1;any=1;
        if(!avp_spirv_read_file(paths[i],models[i],&pending[i],&sizes[i]) ||
           sizes[i]!=stamps[i].bytes)goto fail;
        /* A shader must not be modified while a stream is read. Even if it
         * remains structurally valid, a torn stage is not safe to submit. */
        AvpHybridShaderStamp after={0};
        if(!stamp_file(paths[i],&after)||!same_stamp(&after,&stamps[i]))goto fail;
    }
    if(any){
        for(unsigned i=0;i<AVP_HYBRID_SHADER_COUNT;i++)if(changed[i]){
            AvpHybridShaderEntry *entry=&cache->entries[i];
            free(entry->words);
            entry->words=pending[i];pending[i]=NULL;
            entry->bytes=sizes[i];
            entry->stamp=stamps[i];
            strcpy(entry->path,paths[i]);
        }
        cache->updates++;
    }
    return 1;
fail:
    for(unsigned i=0;i<AVP_HYBRID_SHADER_COUNT;i++)free(pending[i]);
    return 0;
}
