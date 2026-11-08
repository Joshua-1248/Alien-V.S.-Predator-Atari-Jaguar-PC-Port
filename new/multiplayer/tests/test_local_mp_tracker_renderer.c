/* Actual hosted Jaguar renderer distance handoff, not a simulated scanner. */
#include "../../../src/jaguar/avp_gpu/software_renderer.c"
#include <stdio.h>
/* The unmodified source AMP allocator's live array; all-zero means no
 * active native enemies in these independently constructed view tests. */
AvpAmp level1amps[AVP_NUM_AMPS];
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);return 1;}} while(0)
int main(void)
{
    AvpAvpGpuState v1={0},v2={0};
    const u32 far=15u<<16;
    /* Original single-player path has no player contacts. */
    CHECK(avp_gpu_tracker_nearest_contact(0u,0u,0u)==far);
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v1,0u,0u,0u)==far);
    v1.map_teammate_count=1;
    v1.map_teammates[0].x=3u<<16;
    v1.map_teammates[0].y=0;
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v1,0u,0u,0u)==9u<<16);
    /* R23: automap-only toggle must not drop peers from the Marine radar. */
    avp_gpu_show_map_teammates(&v1,0);
    CHECK(v1.map_hide_teammates && v1.map_teammate_count==1u);
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v1,0u,0u,0u)==9u<<16);
    avp_gpu_show_map_teammates(&v1,1);
    CHECK(!v1.map_hide_teammates && v1.map_teammate_count==1u);
    /* Another independent viewport must not inherit P1's scanner dots. */
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v2,0u,0u,0u)==far);
    /* A contact on the exact 15m cutoff does not trigger the tracker. */
    v1.map_teammates[0].x=5u<<16;
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v1,0u,0u,0u)==far);
    /* The closest of multiple same-level participants drives sound/ping. */
    v1.map_teammate_count=3;
    v1.map_teammates[0].x=4u<<16;
    v1.map_teammates[1].x=2u<<16;
    v1.map_teammates[2].x=3u<<16;
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v1,0u,0u,0u)==6u<<16);
    v1.map_teammate_count=0;
    CHECK(avp_gpu_tracker_nearest_contact_for_view(&v1,0u,0u,0u)==far);
    /* R49: the Jaguar scanner retains the authoritative contact even if its
     * presentation-only automap dot moves between simulation snapshots. */
    {
        AvpGpuMapTeammate raw[2]={{2u<<16,3u<<16},{4u<<16,5u<<16}};
        AvpGpuMapTeammate shown[2]={{(2u<<16)+0x2000u,3u<<16},
                                    {(4u<<16)+0x1000u,5u<<16}};
        CHECK(avp_gpu_set_map_teammates(&v1,raw,2u));
        CHECK(v1.map_teammate_count==2u);
        CHECK(v1.map_teammates[0].x==raw[0].x);
        CHECK(v1.map_teammates_draw[0].x==raw[0].x);
        CHECK(avp_gpu_set_map_teammates_draw(&v1,shown,2u));
        CHECK(v1.map_teammates_draw[0].x==shown[0].x);
        CHECK(v1.map_teammates[0].x==raw[0].x);
        CHECK(v1.map_teammates[1].y==raw[1].y);
        CHECK(!avp_gpu_set_map_teammates_draw(&v1,shown,1u));
        CHECK(v1.map_teammates_draw[0].x==shown[0].x);
        CHECK(!avp_gpu_set_map_teammates_draw(&v1,NULL,2u));
        CHECK(avp_gpu_set_map_teammates(&v2,raw,1u));
        CHECK(v2.map_teammates_draw[0].x==raw[0].x);
        CHECK(v2.map_teammates_draw[0].x!=v1.map_teammates_draw[0].x);
        avp_gpu_show_map_teammates(&v1,0);
        CHECK(v1.map_hide_teammates);
        CHECK(v1.map_teammates[0].x==raw[0].x);
        CHECK(v1.map_teammates_draw[0].x==shown[0].x);
        CHECK(avp_gpu_set_map_teammates(&v1,raw,1u));
        CHECK(v1.map_teammates_draw[0].x==raw[0].x);
        CHECK(avp_gpu_set_map_teammates(&v1,NULL,0u));
        CHECK(v1.map_teammate_count==0u);
        CHECK(avp_gpu_set_map_teammates_draw(&v1,NULL,0u));
    }
    puts("R49 real scanner preserved; independent visual map-dot positions and reset PASS");
    return 0;
}
