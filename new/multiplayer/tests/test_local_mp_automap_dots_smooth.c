/* R49: presentation-only 10 Hz participant dots, no ROM dependency. */
#include "jaguar/avp_gpu/presentation_interp.h"
#include <stdint.h>
#include <stdio.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"dot interpolation failed at line %d: %s\n",__LINE__,#c);return 1;}}while(0)
int main(void)
{
    AvpPresentationPose before={2u<<16,3u<<16,0},
                        after={(2u<<16)+0x4000u,(3u<<16)+0x2000u,0},shown;
    /* All presentation phases must cover the full 10 Hz motion, not repeat
     * the single simulation position until the next tick. */
    for(unsigned step=0;step<=10u;step++){
        const double alpha=(double)step/10.0;
        const uint32_t x=before.x+(uint32_t)(0x4000u*alpha);
        const uint32_t y=before.y+(uint32_t)(0x2000u*alpha);
        CHECK(avp_presentation_interpolate_pose(&before,&after,alpha,&shown));
        CHECK(shown.x==x && shown.y==y);
    }
    /* No extrapolation beyond the game-clock endpoints. */
    CHECK(avp_presentation_interpolate_pose(&before,&after,-1.0,&shown));
    CHECK(shown.x==before.x);
    CHECK(avp_presentation_interpolate_pose(&before,&after,2.0,&shown));
    CHECK(shown.x==after.x);
    /* In-flight diagonal motion uses the same interpolation as actor poses. */
    before.x=0x00108000u;before.y=0x00208000u;
    after.x=0x0010a000u;after.y=0x0020a000u;
    CHECK(avp_presentation_interpolate_pose(&before,&after,0.5,&shown));
    CHECK(shown.x==0x00109000u && shown.y==0x00209000u);
    /* Teleport/level transfer: no ghost dot sliding across the map. */
    before.x=2u<<16;before.y=3u<<16;
    after.x=7u<<16;after.y=8u<<16;
    CHECK(!avp_presentation_interpolate_pose(&before,&after,0.5,&shown));
    CHECK(shown.x==after.x && shown.y==after.y);
    /* Face discontinuities are guarded just like the world actor sprites. */
    before=after;before.angle=0;
    after.angle=0x70000000u;
    CHECK(!avp_presentation_interpolate_pose(&before,&after,0.5,&shown));
    CHECK(shown.angle==after.angle);
    puts("R49 automap dot interpolation: 11 phases, diagonal, endpoints, teleports, facing snaps PASS");
    return 0;
}
