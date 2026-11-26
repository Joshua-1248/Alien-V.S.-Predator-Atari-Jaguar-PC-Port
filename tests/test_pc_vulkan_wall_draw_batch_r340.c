/* RE47 R340: checked GPU upload->render mapping, no SDK/ROM required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pc/pc_vulkan_wall_draw_batch.h"

static void word(uint8_t *p,uint32_t n)
{
    p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);
    p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);
}
static void make_wire(uint8_t *data,uint32_t columns,int empty)
{
    memset(data,0,32u+(size_t)columns*44u);
    word(data,AVP_VK_WALL_PROJECTION_MAGIC);
    word(data+4,AVP_VK_WALL_PROJECTION_VERSION);
    word(data+8,columns);word(data+12,32u);
    word(data+16,44u);word(data+20,32u);
    word(data+24,empty?0u:columns);
    if(empty)return;
    for(uint32_t x=0u;x<columns;x++) {
        uint8_t *rec=data+32u+(size_t)x*44u;
        word(rec,1u);
        word(rec+4,x<columns/2u?5u:6u);
        word(rec+8,x<columns/2u?1u:2u);
        word(rec+12,4u);word(rec+16,28u);
        word(rec+20,32772u);word(rec+24,24u);
        word(rec+28,x%16u);
        word(rec+32,16u);word(rec+36,16u);
        word(rec+40,AVP_WORLD_IMAGE_RGB8_ALPHA8);
    }
}
static void test_draws(void)
{
    uint8_t wire[32u+44u*32u];
    static int handles[2];
    AvpVkWallImageBank bank={0};
    AvpVkWallDrawBatch draws={0};
    make_wire(wire,32u,0);
    assert(avp_vk_wall_projection_validate(wire,sizeof(wire)));
    bank.count=2u;
    bank.order[0]=5u;bank.order[1]=6u;
    bank.uploaded_bytes=2048u;
    bank.slots[5]=(AvpVkWallImageEntry){&handles[0],16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    bank.slots[6]=(AvpVkWallImageEntry){&handles[1],16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    assert(avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    assert(draws.count==2u && draws.active_columns==32u &&
           draws.width==32u && draws.height==32u);
    assert(draws.draws[0].slot==5u && draws.draws[0].columns==16u &&
           draws.draws[1].slot==6u && draws.draws[1].columns==16u);
    /* Stale descriptor slots, byte counts and duplicate ownership fail closed. */
    bank.slots[6].resource=&handles[0];
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    assert(!draws.count);
    bank.slots[6].resource=&handles[1];
    bank.uploaded_bytes=1u;
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    bank.uploaded_bytes=2048u;
    bank.order[1]=5u;
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    bank.order[1]=6u;
    bank.slots[6].width=64u;
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    bank.slots[6].width=16u;
    bank.slots[7].resource=&handles[1];
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    bank.slots[7].resource=NULL;
    assert(avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    word(wire+24,31u);
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    word(wire+24,32u);
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire)-1u,&bank,&draws));
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),NULL,&draws));
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,NULL));
    make_wire(wire,32u,1);
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    memset(&bank,0,sizeof(bank));
    assert(avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
    assert(!draws.count && !draws.active_columns && draws.width==32u);
    bank.uploaded_bytes=1024u;
    assert(!avp_vk_wall_draw_batch_build(wire,sizeof(wire),&bank,&draws));
}
int main(void)
{
    test_draws();
    puts("R340 source-image bank / validated frame draw binding PASS");
    return 0;
}
