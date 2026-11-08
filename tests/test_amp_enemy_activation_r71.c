/* RE33 R71: The opt-in pursuit wake gate retains the original 1152-unit
 * square range and multiplies ONLY that gate (2x / 3x / 4x / unlimited). */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos=0x00188000u,y_pos=0x00188000u,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy=90,player_type=PT_HUMAN,use_cocoon;
volatile u16 seed0,seed1;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return ++seed0;}

static unsigned pursuits;
static void on_pursuit(AvpAmp *a,s32 x,s32 y){(void)a;(void)x;(void)y;++pursuits;}
static int allows(unsigned mode,unsigned x_units,unsigned y_units)
{
    AvpAmp *a;
    InitAMPs();build_level();
    a=amp_req();
    a->creature=AC_ALIEN;
    a->energy=a->oldenergy=100;
    a->xpos=(s32)(x_pos+(x_units<<9));
    a->ypos=(s32)(y_pos+(y_units<<9));
    a->xvel=0;a->yvel=0;
    avp_amp_set_enemy_activation_distance(mode);
    pursuits=0;
    chase_player(a);
    return pursuits!=0;
}
int main(void)
{
    avp_amp_set_chase_callback(on_pursuit);
    assert(avp_amp_enemy_activation_distance()==0u);
    for(unsigned mode=0;mode<4;mode++){
        unsigned cutoff=1152u*(mode+1u);
        assert(allows(mode,cutoff,0));
        assert(!allows(mode,cutoff+1u,0));
        assert(allows(mode,0,cutoff));
        assert(!allows(mode,0,cutoff+1u));
        /* Retail wake region is a square; both axes independently bounded. */
        assert(allows(mode,cutoff,cutoff));
    }
    assert(allows(4,20000,20000));
    assert(allows(4,60000,20000));
    avp_amp_set_enemy_activation_distance(100u);
    assert(avp_amp_enemy_activation_distance()==0u);
    assert(!allows(0,1153u,0));
    puts("R71 original / 2x / 3x / 4x / unlimited enemy chase activation PASS");
    return 0;
}
