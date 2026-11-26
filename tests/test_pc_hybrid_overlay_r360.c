#include "pc/pc_hybrid_overlay.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_partial_and_reconstruct(void)
{
    AvpHybridOverlay f={0};
    uint32_t final[3*4] = {
        0xdead1122u,0x00010203u,0x00020304u,0x00030405u,
        0x00040506u,0x00050607u,0x00060708u,0x00070809u,
        0x0008090au,0x00090a0bu,0x000a0b0cu,0x000b0c0du
    };
    uint32_t restore[12];
    assert(avp_hybrid_overlay_ensure(&f,4u,3u));
    for(uint32_t i=0;i<12u;i++)f.world[i]=final[i]&0xffffffu;
    /* Only two opaque actor/HUD changes should be uploaded. */
    final[1]=0x7f102030u;
    final[10]=0x33fedcbau;
    assert(avp_hybrid_overlay_finish(&f,final,4u,75u));
    assert(f.valid && f.changed==2u && f.transparent==10u);
    assert(f.min_x==1u && f.min_y==0u && f.max_x==3u && f.max_y==3u);
    assert(f.foreground[0]==0u && f.foreground[1]==0xff102030u);
    assert(f.foreground[10]==0xfffedcbau && f.foreground[11]==0u);
    assert(!avp_hybrid_overlay_reconstruct(&f,restore,11u));
    assert(avp_hybrid_overlay_reconstruct(&f,restore,12u));
    for(uint32_t i=0;i<12u;i++)assert(restore[i]==(final[i]&0xffffffu));
    avp_hybrid_overlay_invalidate(&f);
    assert(!avp_hybrid_overlay_reconstruct(&f,restore,12u));
    avp_hybrid_overlay_destroy(&f);
}
static void test_coverage_fail_closed(void)
{
    AvpHybridOverlay f={0};
    uint32_t final[4]={1,2,3,4};
    uint32_t dest[4]={0};
    assert(avp_hybrid_overlay_ensure(&f,2u,2u));
    memset(f.world,0,4*sizeof(uint32_t));
    assert(!avp_hybrid_overlay_finish(&f,final,2u,75u));
    assert(!f.valid && f.changed==4u && f.transparent==0u);
    assert(!avp_hybrid_overlay_reconstruct(&f,dest,4u));
    assert(avp_hybrid_overlay_finish(&f,final,2u,100u));
    assert(avp_hybrid_overlay_reconstruct(&f,dest,4u));
    for(unsigned i=0;i<4u;i++)assert(dest[i]==final[i]);
    assert(!avp_hybrid_overlay_finish(&f,final,2u,101u));
    assert(!f.valid);
    /* Safe resize and no stale output after either a failed or good resize. */
    assert(avp_hybrid_overlay_ensure(&f,5u,2u));
    assert(!f.valid && f.width==5u && f.height==2u);
    assert(!avp_hybrid_overlay_ensure(&f,UINT_MAX,UINT_MAX));
    assert(!f.valid && f.width==5u && f.height==2u);
    assert(!avp_hybrid_overlay_finish(&f,final,2u,75u));
    avp_hybrid_overlay_destroy(&f);
}
static void test_stride_and_unmodified(void)
{
    AvpHybridOverlay f={0};
    uint32_t final[6]={10,20,999,30,40,999};
    uint32_t out[4]={0};
    assert(avp_hybrid_overlay_ensure(&f,2u,2u));
    f.world[0]=10;f.world[1]=20;f.world[2]=30;f.world[3]=40;
    assert(avp_hybrid_overlay_finish(&f,final,3u,0u));
    assert(f.valid && f.changed==0u && f.transparent==4u);
    assert(f.max_x==0 && f.max_y==0);
    assert(avp_hybrid_overlay_reconstruct(&f,out,4u));
    for(unsigned i=0;i<4u;i++)assert(out[i]==f.world[i]);
    assert(!avp_hybrid_overlay_finish(&f,final,1u,100u));
    avp_hybrid_overlay_destroy(&f);
    assert(!avp_hybrid_overlay_ensure(NULL,2u,2u));
}
int main(void)
{
    test_partial_and_reconstruct();
    test_coverage_fail_closed();
    test_stride_and_unmodified();
    puts("R360 hybrid CPU foreground extraction and fallback: PASS");
    return 0;
}
