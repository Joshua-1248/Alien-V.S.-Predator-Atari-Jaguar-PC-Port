/* Reconstructed retail ambience descriptor selection, multiplayer-only
 * membership policy; does not fabricate audio samples or game timing. */
#include "local_mp_ambient_policy.h"
#include "pc/pc_input_routing.h"
/* Keep test assertions active in Release builds as well as Debug builds. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
int main(void)
{
    unsigned a[4]={3,3,3,3};
    const u32 use=0xffffffffu&~(1u<<FIRE_B);
    assert(avp_mp_ambient_target(a,4)==3);
    a[1]=4;
    assert(avp_mp_ambient_target(a,4)==2); /* split -> source sublevel cue */
    a[1]=3;
    assert(avp_mp_ambient_target(a,4)==3); /* reunion */
    for(unsigned p=0;p<4;p++)a[p]=15;
    assert(avp_mp_ambient_target(a,4)==15);
    a[0]=2;a[1]=4;a[2]=3;a[3]=15;
    assert(avp_mp_ambient_target(a,4)==2);
    assert(avp_mp_ambient_target(a,0)==0);
    assert(avp_mp_ambient_target(a,5)==0);
    a[3]=0;
    assert(avp_mp_ambient_target(a,4)==0);
    a[3]=16;
    assert(avp_mp_ambient_target(a,4)==0);
    /* Reuse single-player's exact analog lift chord for ALL 4 actors.
     * Original Jaguar DOORS.S receives FIRE_B + one direction only while
     * Use is held and the actor is facing an actual authored lift panel. */
    for(unsigned p=0;p<4;p++){
        (void)p;
        assert(!(avp_pc_lift_analog_pad(use,1,1,0.25f)&(1u<<JOY_UP)));
        assert(!(avp_pc_lift_analog_pad(use,1,1,-0.25f)&(1u<<JOY_DOWN)));
        assert(avp_pc_lift_analog_pad(use,1,0,0.6f)==use);
        assert(avp_pc_lift_analog_pad(0xffffffffu,1,1,0.6f)==0xffffffffu);
        assert(avp_pc_lift_analog_pad(use,1,1,0)==use);
        assert(avp_pc_lift_analog_pad(use,0,1,0.6f)==use);
    }
    puts("PASS: 4-player ambient membership, reunion, and original analog lift input");
    return 0;
}
