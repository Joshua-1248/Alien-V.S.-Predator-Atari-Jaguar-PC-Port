/* R345: no ROM, Vulkan SDK or user-owned artwork needed for this wire test. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_projection.h"
#include "pc/pc_vulkan_wall_slots.h"

static void put32(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8u);
    p[2]=(uint8_t)(v>>16u);p[3]=(uint8_t)(v>>24u);
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8u) |
           ((uint32_t)p[2]<<16u) | ((uint32_t)p[3]<<24u);
}
static void set_col(uint8_t *r,uint32_t slot,uint32_t top,uint32_t bottom,
                    uint32_t wall_height,uint32_t texture_height)
{
    put32(r,1u);put32(r+4u,slot);put32(r+8u,1u);
    put32(r+12u,top);put32(r+16u,bottom);
    put32(r+20u,AVP_VK_WALL_PROJECTION_BASE_BIAS);
    put32(r+24u,wall_height);put32(r+28u,0u);
    put32(r+32u,128u);put32(r+36u,texture_height);
    put32(r+40u,AVP_WORLD_IMAGE_RGB8_ALPHA8);
}
static void test_two_columns_with_mixed_source_heights(void)
{
    AvpVkWallProjectionStream s={0};
    AvpVkWallSlotPlan plan={0};
    const uint32_t width=3u,screen_height=1080u;
    const size_t before=32u+width*48u;
    uint32_t columns=123u,rows=456u,diff=UINT32_MAX;
    uint8_t *bad;
    s.data=(uint8_t *)calloc(1u,before);assert(s.data);
    s.bytes=s.capacity=before;
    put32(s.data,AVP_VK_WALL_PROJECTION_MAGIC);
    put32(s.data+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    put32(s.data+8u,width);put32(s.data+12u,screen_height);
    put32(s.data+16u,48u);put32(s.data+20u,32u);
    put32(s.data+24u,3u);
    set_col(s.data+32u,5u,0u,1080u,1080u,720u);
    set_col(s.data+80u,6u,0u,1080u,1080u,128u);
    set_col(s.data+128u,7u,0u,1080u,1080u,1254u);
    assert(avp_vk_wall_projection_validate(s.data,s.bytes));
    assert(avp_vk_wall_projection_row_parity(s.data,s.bytes,&diff));
    assert(diff>0u);
    assert(avp_vk_wall_projection_upgrade_exact_rows(&s,&columns,&rows));
    assert(columns>=1u && columns<=2u && rows==columns*1080u);
    assert(s.data[4u]==AVP_VK_WALL_PROJECTION_EXACT_VERSION);
    assert(s.bytes==32u+width*52u+(size_t)columns*540u*4u);
    assert(avp_vk_wall_projection_validate(s.data,s.bytes));
    assert(avp_vk_wall_projection_row_parity(s.data,s.bytes,&diff) && !diff);
    assert(avp_vk_wall_slots_collect(s.data,s.bytes,&plan));
    assert(plan.used_count==3u && plan.active_columns==3u);
    assert(get32(s.data+32u+52u+48u)==0u); /* native height no correction */
    for(uint32_t x=0u;x<width;x++){
        const uint8_t *r=s.data+32u+x*52u;
        const uint32_t height=get32(r+36u);
        int actual=-1;
        const double step=(double)height/1080.0;
        double source=0.5*step;
        for(uint32_t y=0u;y<1080u;y++){
            int expected;
            assert(avp_vk_wall_projection_lookup_row(s.data,s.bytes,x,y,&actual));
            if(height<=128u){source+=step;continue;}
            expected=(int)source;
            if(expected>=(int)height)expected=(int)height-1;
            assert(actual==expected);
            source+=step;
        }
        assert(!avp_vk_wall_projection_lookup_row(s.data,s.bytes,x,1080u,&actual));
    }
    bad=(uint8_t *)malloc(s.bytes);assert(bad);
    memcpy(bad,s.data,s.bytes);
    put32(bad+28u,0u);assert(!avp_vk_wall_projection_validate(bad,s.bytes));
    memcpy(bad,s.data,s.bytes);
    put32(bad+32u+48u,(uint32_t)(s.bytes/4u)+1u);
    assert(!avp_vk_wall_projection_validate(bad,s.bytes));
    memcpy(bad,s.data,s.bytes);
    {
        const uint32_t offset=get32(bad+32u+48u);
        if(offset){
            put32(bad+4u*offset,0x0000ffffu);
            assert(!avp_vk_wall_projection_validate(bad,s.bytes));
        }
    }
    assert(!avp_vk_wall_projection_validate(s.data,s.bytes-4u));
    assert(!avp_vk_wall_projection_upgrade_exact_rows(&s,&columns,&rows));
    free(bad);
    avp_vk_wall_projection_destroy(&s);
}
static void test_no_change_to_legacy_frame(void)
{
    AvpVkWallProjectionStream s={0};
    uint8_t wire[80]={0};
    put32(wire,AVP_VK_WALL_PROJECTION_MAGIC);
    put32(wire+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    put32(wire+8u,1u);put32(wire+12u,16u);
    put32(wire+16u,48u);put32(wire+20u,32u);put32(wire+24u,1u);
    set_col(wire+32u,5u,0u,16u,16u,128u);
    s.data=(uint8_t *)malloc(sizeof wire);assert(s.data);
    memcpy(s.data,wire,sizeof wire);s.capacity=s.bytes=sizeof wire;
    assert(avp_vk_wall_projection_validate(s.data,s.bytes));
    assert(avp_vk_wall_projection_upgrade_exact_rows(&s,NULL,NULL));
    assert(s.bytes==sizeof wire && !memcmp(s.data,wire,sizeof wire));
    avp_vk_wall_projection_destroy(&s);
}
int main(void)
{
    test_two_columns_with_mixed_source_heights();
    test_no_change_to_legacy_frame();
    puts("R345 exact high-res row-map v3, mixed slots, corruption and legacy preservation PASS");
    return 0;
}
