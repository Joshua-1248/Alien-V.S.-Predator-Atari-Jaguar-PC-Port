/* Standalone synthetic probe: use the same runtime callback fixture as the
 * existing AMP test, but exercise signed generator-count boundaries. */
#define main amp_fixture_legacy_main
#include "test_amp.c"
#undef main
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    AvpRuntimeOps ops={0};AvpAmp a={0};
    static const uint32_t dist[]={0u,0x10000u,0x1230000u,0x4000000u,0x6000000u};
    uint64_t hash=UINT64_C(14695981039346656037);
    ops.play_sfx_params=gen_mod;avp_runtime_bind(&ops);
    x_pos=y_pos=0x08000000u;
    for(unsigned g=1;g<=8;g++)for(unsigned i=0;i<sizeof(dist)/sizeof(*dist);i++){
        ngens=(u16)g;
        a.xpos=(s32)(x_pos+dist[i]);a.ypos=(s32)y_pos;
        gen_updates=0;generate(&a);a.mode(&a);
        assert(gen_updates==1u);
        int64_t missing=4-(int64_t)g;
        int64_t vol=0x1a00-(int64_t)(dist[i]>>7)-missing*256;
        int64_t pitch=0xe0-(int64_t)(dist[i]>>16)-missing*8;
        if(vol<0)vol=0;
        if(pitch<0x80)pitch=80;
        assert(gen_last_volume==(s32)vol);
        assert(gen_last_pitch==(s32)pitch);
        hash^=(uint32_t)gen_last_volume;hash*=UINT64_C(1099511628211);
        hash^=(uint32_t)gen_last_pitch;hash*=UINT64_C(1099511628211);
    }
    avp_runtime_reset_ops();
    printf("generator alarm: 40 cases hash=%016llx\n",(unsigned long long)hash);
    return 0;
}
