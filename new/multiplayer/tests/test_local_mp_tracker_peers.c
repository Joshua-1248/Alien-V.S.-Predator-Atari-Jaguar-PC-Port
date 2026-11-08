/* R16: same-level living player radar contacts in co-op AND Deathmatch;
 * original 15-m MAZE4 geometry, no corpses/self/other-level ghosts. */
#include "../local_mp_tracker_peers.h"
#include "jaguar/avp_gpu/tracker_contact_math.h"
#include <stdio.h>
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);return 1;} } while(0)
int main(void)
{
    int level_a=0,level_b=0;
    const void *a=&level_a,*b=&level_b;
    /* No team/deathmatch filter: both modes see all other living players
     * who actually occupy the same independently simulated level. */
    CHECK(avp_mp_tracker_peer_eligible(0,1,a,a,0,100,0));
    CHECK(avp_mp_tracker_peer_eligible(1,0,a,a,0,100,0));
    CHECK(avp_mp_tracker_peer_eligible(0,2,a,a,0,100,0));
    CHECK(avp_mp_tracker_peer_eligible(0,3,a,a,0,100,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,0,a,a,0,100,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,a,b,0,100,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,a,NULL,0,100,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,NULL,a,0,100,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,a,a,1,100,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,a,a,0,0,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,a,a,0,-1,0));
    CHECK(!avp_mp_tracker_peer_eligible(0,1,a,a,0,100,1));
    /* Exact Jaguar cutoff: less than 15 metres; equal is absent. */
    CHECK(avp_gpu_tracker_contact_metres(4.0,0.0)==12.0);
    CHECK(avp_gpu_tracker_contact_metres(5.0,0.0)==15.0);
    CHECK(avp_gpu_tracker_contact_metres(3.0,3.0)==12.0);
    CHECK(avp_gpu_tracker_contact_metres(-3.0,-3.0)==12.0);
    CHECK(avp_gpu_tracker_contact_metres(5.0,0.0)>=15.0);
    CHECK(avp_gpu_tracker_contact_metres(4.99,0.0)<15.0);
    CHECK(avp_gpu_tracker_contact_metres(12.0,0.0)>15.0);
    puts("R16 player radar roster and original 15-m tracker range PASS");
    return 0;
}
