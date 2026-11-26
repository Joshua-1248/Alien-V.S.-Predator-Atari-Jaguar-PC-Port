#include "pc/pc_hybrid_texture_cache.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"R362 cache test: %s:%u (%s)\n",__FILE__,__LINE__,#c);return 1;}}while(0)

int main(void)
{
    uint32_t a[]={0x00112233u,0x44556677u,0x8899aabbu,0xff000000u};
    uint32_t b[]={0x10u,0x20u};
    AvpVkLayeredFrame f={0};
    AvpHybridTextureCache cache={0};
    CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));
    CHECK(!avp_hybrid_texture_cache_capture(&cache,&f));
    f.slots.used_count=2u;
    f.slots.active_columns=2u;
    f.slots.order[0]=1u;
    f.slots.order[1]=5u;
    f.slots.slots[1]=(AvpVkWallSlot){1u,1u,2u,2u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    f.slots.slots[5]=(AvpVkWallSlot){1u,1u,2u,1u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    f.pixels[1]=(AvpVkWallPixelSource){a,2u,2u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    f.pixels[5]=(AvpVkWallPixelSource){b,2u,1u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    CHECK(avp_hybrid_texture_cache_capture(&cache,&f));
    CHECK(cache.bytes==24u);
    CHECK(avp_hybrid_texture_cache_matches(&cache,&f));
    a[1]^=1u;CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));a[1]^=1u;
    b[0]^=0x100u;CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));b[0]^=0x100u;
    CHECK(avp_hybrid_texture_cache_matches(&cache,&f));
    f.slots.slots[5].encoding=AVP_WORLD_IMAGE_RGB8_ALPHA8;
    CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));
    CHECK(!avp_hybrid_texture_cache_capture(&cache,&f)); /* source disagrees */
    f.slots.slots[5].encoding=AVP_WORLD_IMAGE_CRY16_ALPHA8;
    CHECK(avp_hybrid_texture_cache_matches(&cache,&f)); /* transactional failure */
    f.slots.order[0]=5u;f.slots.order[1]=1u;
    CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));
    CHECK(!avp_hybrid_texture_cache_capture(&cache,&f));
    f.slots.order[0]=1u;f.slots.order[1]=5u;
    CHECK(avp_hybrid_texture_cache_matches(&cache,&f));
    f.slots.slots[1].width=3u;
    CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));
    CHECK(!avp_hybrid_texture_cache_capture(&cache,&f));
    f.slots.slots[1].width=2u;
    CHECK(avp_hybrid_texture_cache_matches(&cache,&f));
    f.slots.used_count=1u;f.slots.active_columns=1u;
    f.slots.slots[5].used=0u;
    CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));
    CHECK(avp_hybrid_texture_cache_capture(&cache,&f));
    CHECK(cache.bytes==16u);
    CHECK(avp_hybrid_texture_cache_matches(&cache,&f));
    /* R376: a texture remains valid even after disappearing from view,
     * while changed bytes or metadata fail exact reuse. */
    CHECK(avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    CHECK(!avp_hybrid_texture_cache_slot_matches(&cache,&f,5u));
    CHECK(avp_hybrid_texture_cache_slot_capture(&cache,&f,1u));
    CHECK(avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    a[0]^=0x10u;
    CHECK(!avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    CHECK(avp_hybrid_texture_cache_slot_capture(&cache,&f,1u));
    CHECK(avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    a[0]^=0x10u;
    CHECK(!avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    CHECK(avp_hybrid_texture_cache_slot_capture(&cache,&f,1u));
    CHECK(avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    f.slots.slots[1].width=3u;
    CHECK(!avp_hybrid_texture_cache_slot_matches(&cache,&f,1u));
    CHECK(!avp_hybrid_texture_cache_slot_capture(&cache,&f,1u));
    f.slots.slots[1].width=2u;
    /* Disappearing slot 5 must not invalidate slot 1 or the cached slot 5.
     * Reappearance compares source CONTENT, not the current visible plan. */
    {
        AvpHybridTextureCache persistent={0};
        f.slots.used_count=2u;f.slots.active_columns=2u;
        f.slots.slots[5]=(AvpVkWallSlot){1u,1u,2u,1u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
        CHECK(avp_hybrid_texture_cache_slot_capture(&persistent,&f,1u));
        CHECK(avp_hybrid_texture_cache_slot_capture(&persistent,&f,5u));
        CHECK(avp_hybrid_texture_cache_slot_matches(&persistent,&f,5u));
        f.slots.used_count=1u;f.slots.active_columns=1u;
        f.slots.slots[5].used=0u;
        CHECK(avp_hybrid_texture_cache_slot_matches(&persistent,&f,1u));
        CHECK(persistent.slots[5].words!=NULL);
        f.slots.used_count=2u;f.slots.active_columns=2u;
        f.slots.slots[5].used=1u;
        CHECK(avp_hybrid_texture_cache_slot_matches(&persistent,&f,5u));
        b[0]^=1u;
        CHECK(!avp_hybrid_texture_cache_slot_matches(&persistent,&f,5u));
        CHECK(avp_hybrid_texture_cache_slot_matches(&persistent,&f,1u));
        b[0]^=1u;
        avp_hybrid_texture_cache_invalidate(&persistent);
        f.slots.used_count=1u;f.slots.active_columns=1u;
        f.slots.slots[5].used=0u;
    }
    f.slots.used_count=0u;f.slots.active_columns=0u;
    CHECK(!avp_hybrid_texture_cache_matches(&cache,&f));
    avp_hybrid_texture_cache_invalidate(&cache);
    CHECK(!cache.valid&&!cache.slots[1].words&&!cache.slots[5].words);
    avp_hybrid_texture_cache_invalidate(&cache);
    puts("R362 exact-source GPU texture retention policy PASS");
    return 0;
}
