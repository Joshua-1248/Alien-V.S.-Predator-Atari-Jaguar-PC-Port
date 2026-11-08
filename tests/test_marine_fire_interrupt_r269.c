/* R269: production Marine pulse state machine + original human death transition.
 * Fatal hits abort the pending burst immediately in Alien/Predator campaigns,
 * but the optional enhancement must preserve default Jaguar behavior, all
 * other Marine damage paths and the independent R268 line-of-sight option. */
#include "amp.h"
#include "player.h"
#include "collision.h"
#include "hud.h"
#include <assert.h>
#include <stdio.h>

s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock;u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
static int los_calls;
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;++los_calls;return 0;}
s32 score;u32 fire_distance;
u32 x_pos,y_pos;u16 invisflag;
s16 player_energy,player_type,use_cocoon;
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static u32 beam_sample(void){return 2;}

static AvpAmp *start_burst(int species,int interrupt,int los)
{
    AvpAmp *a;
    player_type=(s16)species;
    x_pos=0x00180000u;y_pos=0x00180000u;
    invisflag=0;player_energy=500;los_calls=0;
    avp_amp_set_marine_fire_interrupt(interrupt);
    avp_amp_set_marine_pulse_los_hits(los);
    InitAMPs();build_level();
    a=amp_req();
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->xpos=0x00140000;a->ypos=0x00180000;
    a->xvel=0x1600;a->yvel=0x1600;
    a->energy=a->oldenergy=50;a->timer=1;
    a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    amp_setgrid(a);
    avp_amp_set_beam_counter(2);
    chase_player(a);
    assert(a->animseq==AS_FIGHT1 && a->mode!=chase_player);
    assert(player_energy==500);
    return a;
}

static void fatal_mid_burst(int species,int stage,int los)
{
    AvpAmp *a=start_burst(species,1,los);
    for(int n=0;n<stage;n++){
        a->mode(a);assert(a->animseq==AS_FIGHT1);
    }
    /* A fatal weapon strike happens between the 10 Hz AMP updates. */
    a->energy=0;
    a->mode(a);
    assert(a->flags&(1u<<AMP_DEAD));
    assert(!(a->flags&(1u<<AMP_KILLABLE)));
    assert(a->animseq==AS_DEATH);
    assert(a->mode!=chase_player);
    assert(a->astype==53); /* Original dead-human template. */
    assert(player_energy==500);
    /* Death-mode progression must never resume the pending pulse burst. */
    for(int tick=0;tick<30;tick++){
        if(a->host_static||!a->mode)break;
        a->mode(a);
        assert(player_energy==500);
    }
}

int main(void)
{
    AvpAmp *a;
    avp_amp_set_beam_weapon_sample(beam_sample);
    assert(!avp_amp_marine_fire_interrupt());
    for(int i=0;i<2;i++){
        int species=i?PT_PREDATOR:PT_ALIEN;
        /* Each meaningful start/middle/end animation phase. */
        fatal_mid_burst(species,0,0);
        fatal_mid_burst(species,3,0);
        fatal_mid_burst(species,7,1);
        /* Final-frame energy snapshot may equal zero: death STILL wins. */
        a=start_burst(species,1,0);
        a->timer=8;a->energy=a->oldenergy=0;
        a->mode(a);
        assert(a->flags&(1u<<AMP_DEAD));
        assert(player_energy==500);
    }
    /* Non-fatal hits retain source recoil, never cause a death. */
    a=start_burst(PT_ALIEN,1,0);
    a->energy=30;
    a->mode(a);
    assert(a->animseq==AS_KNOCKBACK && !(a->flags&(1u<<AMP_DEAD)));
    assert(player_energy==500);
    /* Living enemy still finishes pulse animation and deals original damage. */
    a=start_burst(PT_PREDATOR,1,0);
    for(int tick=0;tick<15 && a->mode!=chase_player;tick++)a->mode(a);
    assert(a->mode==chase_player && player_energy==400);
    /* Disabled option: preserve original even in the fatal/snapshot edge case. */
    a=start_burst(PT_ALIEN,0,0);
    a->timer=8;a->energy=a->oldenergy=0;
    a->mode(a);
    assert(a->mode==chase_player && player_energy==400);
    /* With option disabled, source's ordinary recoil behavior remains intact. */
    a=start_burst(PT_PREDATOR,0,0);
    a->energy=0;
    a->mode(a);
    assert(a->animseq==AS_KNOCKBACK && !(a->flags&(1u<<AMP_DEAD)));
    /* Scope check: campaign where the player is Marine is unaffected. */
    a=start_burst(PT_HUMAN,1,0);
    a->energy=0;
    a->mode(a);
    assert(a->animseq==AS_KNOCKBACK && !(a->flags&(1u<<AMP_DEAD)));
    /* Multiplayer scopes bind an ACTUAL Marine target, even if the selected
     * campaign is Marine. Fatal hit must still interrupt the enemy burst. */
    avp_amp_set_marine_multiplayer_target(1);
    fatal_mid_burst(PT_HUMAN,0,0);
    fatal_mid_burst(PT_HUMAN,5,1);
    avp_amp_set_marine_multiplayer_target(0);
    /* Leaving the co-op scope must preserve single-player species gating. */
    a=start_burst(PT_HUMAN,1,0);
    a->energy=0;a->mode(a);
    assert(!(a->flags&(1u<<AMP_DEAD)));
    avp_amp_set_marine_fire_interrupt(0);
    avp_amp_set_marine_pulse_los_hits(0);
    avp_amp_set_beam_weapon_sample(NULL);
    puts("R269 real Marine fatal interrupt and original death regression PASS");
    return 0;
}
