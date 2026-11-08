/* R268: drive the production AMP Marine ranged attack and finish frames.
 * The optional final-frame LineOfSight recheck must affect ONLY the authored
 * hitscan pulse-rifle damage against Alien/Predator players. */
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
static int los_blocked,los_calls;
static int multiplayer_target;
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;++los_calls;return los_blocked;}
s32 score;u32 fire_distance;
u32 x_pos,y_pos;u16 invisflag;
s16 player_energy,player_type,use_cocoon;
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static u32 sampled_beam=2;
static u32 beam_sample(void){return sampled_beam;}

static void run_burst(int species,int enhancement,int blocked_after_start,int expected_health)
{
    AvpAmp *a;
    player_type=(s16)species;
    x_pos=0x00180000u;y_pos=0x00180000u;
    invisflag=0;
    player_energy=500;
    los_blocked=0;los_calls=0;
    avp_amp_set_marine_pulse_los_hits(enhancement);
    assert(avp_amp_marine_pulse_los_hits()==enhancement);
    InitAMPs();build_level();
    a=amp_req();
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->xpos=0x00140000;a->ypos=0x00180000;
    a->xvel=0x1600;a->yvel=0x1600;
    a->energy=a->oldenergy=50;a->timer=1;
    a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    amp_setgrid(a);
    avp_amp_set_beam_counter(2);
    sampled_beam=2;
    chase_player(a);
    assert(a->animseq==AS_FIGHT1);
    assert(los_calls==1);
    assert(player_energy==500);
    los_blocked=blocked_after_start;
    for(int tick=0;tick<15 && a->mode!=chase_player;tick++){
        a->mode(a);
        if(a->mode!=chase_player)assert(player_energy==500);
    }
    assert(a->mode==chase_player);
    assert(player_energy==expected_health);
    assert(los_calls==(1+(enhancement&&
        (species==PT_ALIEN||species==PT_PREDATOR||
         (species==PT_HUMAN && multiplayer_target))?1:0)));
}

int main(void)
{
    avp_amp_set_beam_weapon_sample(beam_sample);
    assert(!avp_amp_marine_pulse_los_hits()); /* OFF by default */
    run_burst(PT_ALIEN,0,1,400); /* Jaguar: wall no protection after firing began */
    run_burst(PT_ALIEN,1,1,500); /* enhancement: newly blocked -> MISS */
    run_burst(PT_PREDATOR,1,1,500);
    run_burst(PT_ALIEN,1,0,400); /* no new obstruction -> normal hit */
    run_burst(PT_PREDATOR,1,0,400);
    run_burst(PT_HUMAN,1,1,400); /* species scope: no Marine campaign change */
    run_burst(PT_PREDATOR,0,1,400); /* OFF restores Jaguar */
    /* Native multiplayer scopes apply LOS to their current Marine target,
     * without broadening the historical single-player Marine campaign. */
    multiplayer_target=1;avp_amp_set_marine_multiplayer_target(1);
    run_burst(PT_HUMAN,1,1,500);
    run_burst(PT_HUMAN,1,0,400);
    multiplayer_target=0;avp_amp_set_marine_multiplayer_target(0);
    run_burst(PT_HUMAN,1,1,400);
    avp_amp_set_marine_pulse_los_hits(0);
    avp_amp_set_beam_weapon_sample(NULL);
    puts("R268 real Marine pulse-rifle final-frame LOS enhancement PASS");
    return 0;
}
