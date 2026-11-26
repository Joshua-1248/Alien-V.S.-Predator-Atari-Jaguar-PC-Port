#include "pc/pc_hybrid_texture_cache.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLOT_BYTES (64u*1024u*1024u)
#define MAX_TOTAL_BYTES (512ull*1024u*1024u)

static int slot_bytes(const AvpVkLayeredFrame *frame,uint32_t slot,
                      size_t *out)
{
    const AvpVkWallSlot *spec;
    const AvpVkWallPixelSource *source;
    size_t count;
    if(!frame||!out||slot>=AVP_WORLD_SCENE_IMAGE_SLOTS)return 0;
    spec=&frame->slots.slots[slot];
    source=&frame->pixels[slot];
    if(!spec->used||!source->words||!spec->width||!spec->height||
       spec->width!=source->width||spec->height!=source->height||
       spec->encoding!=source->encoding||
       (spec->encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8 &&
        spec->encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8)||
       spec->width>8192u||spec->height>8192u||
       (size_t)spec->width>SIZE_MAX/(size_t)spec->height/4u)return 0;
    count=(size_t)spec->width*(size_t)spec->height*4u;
    if(count>MAX_SLOT_BYTES)return 0;
    *out=count;
    return 1;
}

static int frame_plan_valid(const AvpVkLayeredFrame *frame,uint64_t *bytes)
{
    uint32_t seen[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint64_t total=0u;
    if(!frame||!bytes||!frame->slots.used_count||
       frame->slots.used_count>AVP_WORLD_SCENE_IMAGE_SLOTS||
       frame->slots.active_columns!=frame->slots.used_count)return 0;
    for(uint32_t i=0u;i<frame->slots.used_count;i++){
        uint32_t slot=frame->slots.order[i];
        size_t size;
        if(slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||seen[slot]||
           (i&&slot<=frame->slots.order[i-1u])||
           !slot_bytes(frame,slot,&size)||total+size>MAX_TOTAL_BYTES)return 0;
        total+=size;seen[slot]=1u;
    }
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if(!!seen[i]!=!!frame->slots.slots[i].used)return 0;
    *bytes=total;
    return 1;
}

void avp_hybrid_texture_cache_invalidate(AvpHybridTextureCache *cache)
{
    if(!cache)return;
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        free(cache->slots[i].words);
    memset(cache,0,sizeof(*cache));
}

int avp_hybrid_texture_cache_matches(const AvpHybridTextureCache *cache,
                                    const AvpVkLayeredFrame *frame)
{
    uint64_t total=0u;
    if(!cache||!cache->valid||!frame_plan_valid(frame,&total)||
       cache->used_count!=frame->slots.used_count||cache->bytes!=total)return 0;
    for(uint32_t i=0u;i<cache->used_count;i++){
        uint32_t slot=frame->slots.order[i];
        const AvpHybridTextureCacheSlot *saved=&cache->slots[slot];
        const AvpVkWallSlot *spec=&frame->slots.slots[slot];
        size_t bytes=0u;
        if(cache->order[i]!=slot||!slot_bytes(frame,slot,&bytes)||
           saved->bytes!=bytes||saved->width!=spec->width||
           saved->height!=spec->height||saved->encoding!=spec->encoding||
           !saved->words||memcmp(saved->words,frame->pixels[slot].words,bytes))
            return 0;
    }
    return 1;
}

int avp_hybrid_texture_cache_capture(AvpHybridTextureCache *cache,
                                    const AvpVkLayeredFrame *frame)
{
    AvpHybridTextureCache *temp=NULL;
    uint64_t bytes=0u;
    if(!cache||!frame_plan_valid(frame,&bytes))return 0;
    temp=(AvpHybridTextureCache*)calloc(1u,sizeof(*temp));
    if(!temp)return 0;
    temp->used_count=frame->slots.used_count;
    temp->bytes=bytes;
    for(uint32_t i=0u;i<temp->used_count;i++){
        uint32_t slot=frame->slots.order[i];
        const AvpVkWallSlot *spec=&frame->slots.slots[slot];
        AvpHybridTextureCacheSlot *saved=&temp->slots[slot];
        size_t size=0u;
        if(!slot_bytes(frame,slot,&size))goto failed;
        saved->words=(uint32_t*)malloc(size);
        if(!saved->words)goto failed;
        memcpy(saved->words,frame->pixels[slot].words,size);
        saved->bytes=size;
        saved->width=spec->width;
        saved->height=spec->height;
        saved->encoding=spec->encoding;
        temp->order[i]=slot;
    }
    temp->valid=1;
    avp_hybrid_texture_cache_invalidate(cache);
    *cache=*temp;
    free(temp);
    return 1;
failed:
    avp_hybrid_texture_cache_invalidate(temp);
    free(temp);
    return 0;
}

/* Store exact original packed texels independently of the visible-slot plan.
 * Never trust a hash, stale pointer or a slot used by a previous level. */
int avp_hybrid_texture_cache_slot_matches(const AvpHybridTextureCache *cache,
    const AvpVkLayeredFrame *frame,uint32_t slot)
{
    const AvpHybridTextureCacheSlot *saved;
    const AvpVkWallSlot *spec;
    size_t bytes=0u;
    if(!cache||!frame||slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||
       !slot_bytes(frame,slot,&bytes))return 0;
    saved=&cache->slots[slot];spec=&frame->slots.slots[slot];
    return saved->words && saved->bytes==bytes &&
        saved->width==spec->width && saved->height==spec->height &&
        saved->encoding==spec->encoding &&
        memcmp(saved->words,frame->pixels[slot].words,bytes)==0;
}
int avp_hybrid_texture_cache_slot_capture(AvpHybridTextureCache *cache,
    const AvpVkLayeredFrame *frame,uint32_t slot)
{
    size_t bytes=0u;
    uint32_t *words;
    AvpHybridTextureCacheSlot *saved;
    const AvpVkWallSlot *spec;
    if(!cache||!frame||slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||
       !slot_bytes(frame,slot,&bytes))return 0;
    words=(uint32_t*)malloc(bytes);
    if(!words)return 0;
    memcpy(words,frame->pixels[slot].words,bytes);
    saved=&cache->slots[slot];spec=&frame->slots.slots[slot];
    free(saved->words);
    saved->words=words;saved->bytes=bytes;
    saved->width=spec->width;saved->height=spec->height;
    saved->encoding=spec->encoding;
    return 1;
}
