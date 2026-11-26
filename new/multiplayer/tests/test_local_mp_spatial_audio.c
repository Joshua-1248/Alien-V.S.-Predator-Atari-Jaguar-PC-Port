#include "jaguar/host/mp_spatial_audio.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpMpSpatialListener v[4]={{0}};
    AvpMpSpatialGain g;
    v[0]=(AvpMpSpatialListener){1u,1u,0,0,0,0u};
    g=avp_mp_spatial_gain(4u,1u,0,0,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(4u,1u,0,6<<16,v);
    CHECK(g.listener==0u && g.right==128u && g.left<g.right);
    g=avp_mp_spatial_gain(4u,1u,0,-(6<<16),v);
    CHECK(g.listener==0u && g.left==128u && g.right<g.left);
    g=avp_mp_spatial_gain(4u,1u,0,12<<16,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    g=avp_mp_spatial_gain(4u,2u,0,0,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    v[1]=(AvpMpSpatialListener){1u,2u,0,0,0,0u};
    g=avp_mp_spatial_gain(4u,2u,0,0,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    v[1]=(AvpMpSpatialListener){1u,1u,8<<16,0,0,0u};
    g=avp_mp_spatial_gain(4u,1u,8<<16,0,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(0u,1u,0,0,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(15u,1u,8<<16,0,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(36u,1u,0,0,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    /* An AI-driven visible split-screen player IS a listener. Previously
     * AI-controlled P2-P4 were disabled, forcing their effects to fade by
     * distance to human P1 even when their OWN view was on screen. */
    for(unsigned count=2u;count<=4u;count++){
        for(unsigned i=0u;i<4u;i++){
            v[i].active=avp_mp_spatial_view_listener(i,count,1u);
            v[i].level=1u;
            v[i].x=(int32_t)(i*20u)<<16;
            v[i].y=0;v[i].heading=0u;
        }
        for(unsigned owner=1u;owner<count;owner++){
            /* The P1 listener is far away, but owner P2/P3/P4 must hear
             * its own positional voice at full gain in its own viewport. */
            g=avp_mp_spatial_gain(owner,1u,v[owner].x,v[owner].y,v);
            CHECK(g.listener==owner && g.left==256u && g.right==256u);
        }
        CHECK(count==4u || v[count].active==0u);
        /* A headless bot's effects still follow the nearest live screen. */
        g=avp_mp_spatial_gain(12u,1u,v[count-1u].x,v[count-1u].y,v);
        CHECK(g.listener==count-1u && g.left==256u && g.right==256u);
    }
    CHECK(!avp_mp_spatial_view_listener(2u,2u,1u));
    CHECK(!avp_mp_spatial_view_listener(4u,4u,1u));
    CHECK(!avp_mp_spatial_view_listener(1u,2u,0u));
    CHECK(!avp_mp_spatial_view_listener(1u,2u,16u));
    CHECK(avp_mp_spatial_view_listener(1u,2u,1u));
    /* Sounds cannot cross a level boundary just because both viewports
     * are enabled; the correct same-level listener remains eligible. */
    v[0]=(AvpMpSpatialListener){1u,1u,0,0,0,0u};
    v[1]=(AvpMpSpatialListener){1u,2u,40<<16,0,0,0u};
    v[2]=(AvpMpSpatialListener){0u,0u,0,0,0,0u};
    v[3]=(AvpMpSpatialListener){0u,0u,0,0,0,0u};
    g=avp_mp_spatial_gain(1u,2u,40<<16,0,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(1u,1u,40<<16,0,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    /* A viewport spying on P20 listens at P20's real location. A remote
     * P1 effect cannot remain pinned to the original P1 listener. */
    v[0]=(AvpMpSpatialListener){1u,3u,50<<16,2<<16,0u,20u};
    v[1]=(AvpMpSpatialListener){0u,0u,0,0,0u,0u};
    g=avp_mp_spatial_gain(19u,3u,50<<16,2<<16,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(0u,3u,0,0,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    g=avp_mp_spatial_gain(19u,2u,50<<16,2<<16,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    /* P36 has independent attribution; it cannot alias P4 or P20. */
    v[0]=(AvpMpSpatialListener){1u,3u,50<<16,2<<16,0u,36u};
    g=avp_mp_spatial_gain(35u,3u,50<<16,2<<16,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(35u,4u,50<<16,2<<16,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    /* Original lift and duct sounds use the same positional level gating. */
    v[0]=(AvpMpSpatialListener){1u,3u,6<<16,4<<16,0u,36u};
    v[1]=(AvpMpSpatialListener){1u,4u,6<<16,4<<16,0u,2u};
    g=avp_mp_spatial_world_gain(3u,6<<16,4<<16,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_world_gain(4u,6<<16,4<<16,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_gain(35u,3u,6<<16,4<<16,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    /* Switching P1's camera to P2 must make P2's own weapon direct. */
    v[0]=(AvpMpSpatialListener){1u,1u,12<<16,0,0u,2u};
    g=avp_mp_spatial_gain(1u,1u,12<<16,0,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    /* Acid contact SFX is PRIVATE: ordinary nearby same-level players,
     * different-level players and headless bots never hear it. Spycam
     * hears the cue only when observing the affected actor. */
    v[0]=(AvpMpSpatialListener){1u,3u,0,0,0u,1u}; /* P1 owns viewport */
    v[1]=(AvpMpSpatialListener){1u,3u,0,0,0u,2u}; /* P2 owns viewport */
    v[2]=(AvpMpSpatialListener){0u,0u,0,0,0u,0u};
    v[3]=(AvpMpSpatialListener){0u,0u,0,0,0u,0u};
    g=avp_mp_spatial_personal_gain(1u,3u,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_personal_gain(2u,3u,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    g=avp_mp_spatial_personal_gain(1u,4u,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    v[1].active=0u;
    g=avp_mp_spatial_personal_gain(1u,3u,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    v[0].subject_plus_one=20u;v[0].level=5u;
    g=avp_mp_spatial_personal_gain(19u,5u,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_personal_gain(19u,3u,v);
    CHECK(g.listener==4u && !g.left && !g.right);
    /* An AMP is world-owned: never grant it a player self-listener gain.
     * Same-level nearby -> audible, far and cross-level -> silent. */
    v[0]=(AvpMpSpatialListener){1u,3u,0,0,0u,1u};
    v[1]=(AvpMpSpatialListener){1u,4u,0,0,0u,2u};
    g=avp_mp_spatial_world_gain(3u,0,0,v);
    CHECK(g.listener==0u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_world_gain(3u,16<<16,0,v);
    CHECK(g.listener==4u && g.left==0u && g.right==0u);
    g=avp_mp_spatial_world_gain(2u,0,0,v);
    CHECK(g.listener==4u && g.left==0u && g.right==0u);
    g=avp_mp_spatial_world_gain(4u,0,0,v);
    CHECK(g.listener==1u && g.left==256u && g.right==256u);
    g=avp_mp_spatial_world_gain(3u,0,4<<16,v);
    CHECK(g.listener==0u && g.left!=g.right);
    CHECK(avp_mp_world_waveform_gain(3,3,0,0,0,0)==256u);
    CHECK(avp_mp_world_waveform_gain(3,3,0,0,8<<16,0)>0u);
    CHECK(avp_mp_world_waveform_gain(3,3,0,0,12<<16,0)==0u);
    CHECK(avp_mp_world_waveform_gain(3,4,0,0,0,0)==0u);
    puts("PASS spatial audio: player+enemy sources, Predator HUD meter, spycam, private acid, level isolation, bot P36");
    return 0;
}
