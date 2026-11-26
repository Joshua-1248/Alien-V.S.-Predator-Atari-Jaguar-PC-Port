/* R348 SDK-free verifier for the exact source data sent to the optional GPU. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_mixed_fixture.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "pc/pc_vulkan_wall_oracle_capture.h"
#include "pc/pc_vulkan_wall_pixel_oracle.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"

int main(void)
{
    AvpVkWallMixedFixture f={0};
    AvpVkWallOracleCapture snapshot={0};
    AvpVkWallSlotPlan plan={0};
    uint8_t *rgba=NULL,*bgra=NULL;
    const uint32_t w=640u,h=480u;
    const size_t bytes=(size_t)w*h*4u;
    uint32_t diff=UINT32_MAX;
    int row;
    assert(!avp_vk_wall_mixed_fixture_build(5u,h,&f));
    assert(!avp_vk_wall_mixed_fixture_build(w,100u,&f));
    assert(!f.geometry.data && !f.rgb && !f.cry && !f.cry_lut);
    assert(avp_vk_wall_mixed_fixture_build(w,h,&f));
    assert(!avp_vk_wall_mixed_fixture_build(w,h,&f));
    assert(f.geometry.data[4]==AVP_VK_WALL_PROJECTION_EXACT_VERSION);
    assert(f.mapped_columns>0 && f.mapped_rows>0);
    assert(avp_vk_wall_projection_validate(f.geometry.data,f.geometry.bytes));
    assert(avp_vk_wall_projection_row_parity(f.geometry.data,f.geometry.bytes,&diff));
    assert(diff==0u);
    assert(avp_vk_wall_slots_collect(f.geometry.data,f.geometry.bytes,&plan));
    assert(plan.used_count==2u && plan.slots[AVP_VK_MIXED_CRY_SLOT].encoding==1u &&
           plan.slots[AVP_VK_MIXED_RGB_SLOT].encoding==2u);
    assert(avp_vk_wall_projection_lookup_row(f.geometry.data,f.geometry.bytes,
        16u,240u,&row) && row>=0 && row<720);
    assert(f.cry_lut[0x3050u]!=f.cry_lut[0x30e0u]);
    assert(avp_vk_wall_oracle_capture_build(f.geometry.data,f.geometry.bytes,
        f.sources,f.cry_lut,65536u,131072u,&snapshot));
    assert(snapshot.texture_count==2u);
    assert(snapshot.texture_bytes==((16u*128u+16u*720u)*4u));
    rgba=malloc(bytes);bgra=malloc(bytes);
    assert(rgba && bgra);
    assert(avp_vk_wall_pixel_oracle_make(snapshot.wire,snapshot.wire_bytes,
        snapshot.sources,snapshot.owned_cry_lut,AVP_VK_WALL_PIXEL_RGBA8,
        rgba,bytes));
    assert(avp_vk_wall_pixel_oracle_make(snapshot.wire,snapshot.wire_bytes,
        snapshot.sources,snapshot.owned_cry_lut,AVP_VK_WALL_PIXEL_BGRA8,
        bgra,bytes));
    assert(avp_vk_wall_oracle_capture_compare(&snapshot,AVP_VK_WALL_PIXEL_RGBA8,
        rgba,bytes,&diff) && diff==0u);
    assert(avp_vk_wall_oracle_capture_compare(&snapshot,AVP_VK_WALL_PIXEL_BGRA8,
        bgra,bytes,&diff) && diff==0u);
    for(size_t i=0;i<bytes;i+=4u){
        assert(rgba[i]==bgra[i+2u]);assert(rgba[i+1u]==bgra[i+1u]);
        assert(rgba[i+2u]==bgra[i]);assert(rgba[i+3u]==255u);
    }
    /* The GPU oracle must not read the fixture's freed staging resources. */
    avp_vk_wall_mixed_fixture_dispose(&f);
    assert(avp_vk_wall_oracle_capture_compare(&snapshot,AVP_VK_WALL_PIXEL_RGBA8,
        rgba,bytes,&diff) && diff==0u);
    rgba[(240u*w+16u)*4u]^=1u;
    assert(!avp_vk_wall_oracle_capture_compare(&snapshot,AVP_VK_WALL_PIXEL_RGBA8,
        rgba,bytes,&diff) && diff==1u);
    avp_vk_wall_oracle_capture_dispose(&snapshot);
    assert(!snapshot.wire && !snapshot.owned_cry_lut);
    avp_vk_wall_oracle_capture_dispose(&snapshot);
    free(rgba);free(bgra);
    puts("R348 mixed CRY / RGB hi-res / v3 exact rows / source lifetime PASS (CPU only)");
    return 0;
}
