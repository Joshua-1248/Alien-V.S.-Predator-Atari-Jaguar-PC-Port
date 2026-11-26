/* RE39 R180: run the REAL AMP enemy chase and fake-direction mode functions
 * across 68000 ADD.L/SUB.L register boundaries. No cloned production math.
 * Forced blockage is confined to the SafePos test stub, defaulting off. */
#include "amp.h"
#include "collision.h"
#include "player.h"
#include "hud.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock;u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u32 x_pos,y_pos;u16 invisflag;
s32 score;u32 fire_distance;
s16 player_energy,player_type,use_cocoon;
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static unsigned random_choice;
u16 avp_random(void){return (u16)random_choice;}
extern int avp_test_amp_force_blocked;
static s32 observed_x,observed_y;
static unsigned chase_calls;
static void chase_observer(AvpAmp *a,s32 x,s32 y)
{
    (void)a;observed_x=x;observed_y=y;++chase_calls;
}
/* Independent longword oracle: avoid relying on implementation-defined
 * out-of-range unsigned-to-signed conversions. */
static s32 from_bits(u32 v)
{
    return v<=INT32_MAX?(s32)v:(s32)((int64_t)v-INT64_C(4294967296));
}
static void test_detour(int north_south,int positive,int boundary)
{
    AvpAmp *a;
    InitAMPs();build_level();
    a=amp_req();
    assert(a==&level1amps[0]);
    a->mode=chase_player;a->creature=AC_ALIEN;
    a->animseq=AS_RUN;a->energy=a->oldenergy=100;
    a->xvel=a->yvel=0x1800;a->yvector=0;
    a->xpos=0x00028000;a->ypos=0x00028000;
    player_type=PT_HUMAN;discflag=invisflag=0;
    /* X-direction chase picks an N/S random detour, Y-direction picks E/W. */
    x_pos=north_south?0x00068000u:0x00028000u;
    y_pos=north_south?0x00028000u:0x00068000u;
    random_choice=north_south?(positive?1u:0u):(positive?0u:1u);
    avp_test_amp_force_blocked=1;
    chase_player(a);
    assert(a->mode && a->mode!=chase_player);

    /* Switch the actual target into the 68000 register boundary region.
     * The fake mode uses that current player position, not a cached copy. */
    x_pos=north_south?0x00280000u:
        (boundary?(positive?0x7fffff00u:0x80000100u):0x00280000u);
    y_pos=north_south?
        (boundary?(positive?0x7fffff00u:0x80000100u):0x00280000u):0x00280000u;
    {
        const s32 offset=positive ? -0x30000 : 0x30000;
        a->xpos=from_bits(x_pos+(north_south?0u:(u32)offset));
        a->ypos=from_bits(y_pos+(north_south?(u32)offset:0u));
    }
    chase_calls=0;observed_x=observed_y=0;
    a->mode(a); /* actual R179 overflowing fake E/W/N/S code */
    assert(chase_calls==1);
    assert((u32)observed_x==(u32)x_pos+(north_south?0u:(positive?0x10000u:0xffff0000u)));
    assert((u32)observed_y==(u32)y_pos+(north_south?(positive?0x10000u:0xffff0000u):0u));
    avp_test_amp_force_blocked=0;
}
int main(void)
{
    avp_amp_set_chase_callback(chase_observer);
    /* +X/-X and +Y/-Y; all four originally use longword ADD/SUB. */
    for(int boundary=0;boundary<=1;boundary++){
        test_detour(0,1,boundary);
        test_detour(0,0,boundary);
        test_detour(1,1,boundary);
        test_detour(1,0,boundary);
    }
    avp_amp_set_chase_callback(NULL);
    puts("R180 AMP fake-direction ADD.L/SUB.L eight ordinary/boundary states PASS");
    return 0;
}
