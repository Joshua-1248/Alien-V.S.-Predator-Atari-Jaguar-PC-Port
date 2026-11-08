/* Real AMP state transitions for Predator cloak vs enemy Marine, including
 * loss-of-target search, near-field discovery, and sight reacquisition.
 * No host diagnostic instrumentation is inserted into gameplay code. */
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
s16 game_over,key_lock; u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 score;u32 fire_distance;
u32 x_pos,y_pos;u16 invisflag;
s16 player_energy,player_type,use_cocoon;
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static u32 sampled_beam;
static u32 next_beam(void){return sampled_beam;}

static AvpAmp *new_marine(s32 x,s32 y)
{
    AvpAmp *a;
    InitAMPs();build_level();
    a=amp_req();
    a->mode=chase_player;
    a->creature=AC_HUMAN;
    a->xpos=x;a->ypos=y;
    a->xvel=0x1600;a->yvel=0x1600;
    a->animseq=AS_RUN;
    a->energy=a->oldenergy=50;
    a->timer=1; /* enemy Marines evaluate active chase on alternate ticks */
    a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    amp_setgrid(a);
    return a;
}
int main(void)
{
    AvpAmp *a;
    s32 initial_x;
    player_type=PT_PREDATOR;
    x_pos=0x00180000u;y_pos=0x00180000u;
    invisflag=1;
    avp_amp_set_beam_counter(2);
    avp_amp_set_beam_weapon_sample(next_beam);
    sampled_beam=2;
    a=new_marine(0x00100000,0x00100000);
    initial_x=a->xpos;
    /* A distant, invisible Predator must cause a finite search step, not
     * repeated combat reacquisition through unbounded lostplayer recursion. */
    chase_player(a);
    assert(a->mode==lostplayer);
    assert(!(a->flags&(1u<<AMP_PHIT)));
    /* The Marine tick gate is every other logical frame. */
    lostplayer(a);
    assert(a->mode==lostplayer);
    assert(a->xpos!=initial_x || a->ypos!=0x00100000);
    s32 previous=a->xpos;
    lostplayer(a);lostplayer(a);
    assert(a->mode==lostplayer);
    assert(a->xpos!=previous);
    assert(a->animseq!=AS_FIGHT1 && a->animseq!=AS_FIGHT2);
    /* Decloaking exits the original search state and resumes pursuit. */
    invisflag=0;
    lostplayer(a);
    assert(a->mode==chase_player);

    /* Once an invisible Predator is close enough, Marine proximity search
     * sets the original detection flag and permits normal reacquisition. */
    invisflag=1;
    x_pos=0x00180000u;y_pos=0x00180000u;
    a=new_marine((s32)x_pos-0x00001000,(s32)y_pos);
    chase_player(a);
    assert(a->mode==lostplayer);
    lostplayer(a); /* next active Marine tick detects near-field cloak */
    assert(a->flags&(1u<<AMP_PHIT));
    lostplayer(a);
    assert(a->mode==chase_player);
    /* Verify both author-selected ranged attacks through the production
     * chase_player path, not only via the separately exported gofight helper.
     * The second video-beam read chooses the active weapon. */
    invisflag=0;
    x_pos=0x00180000u;y_pos=0x00180000u;
    a=new_marine(0x00140000,0x00180000);
    sampled_beam=2;
    chase_player(a);
    assert(a->animseq==AS_FIGHT1 && a->mode!=chase_player);
    assert(a->timer==1); /* first authored crouch-firing frame */

    a=new_marine(0x00140000,0x00180000);
    sampled_beam=3;
    chase_player(a);
    assert(a->animseq==AS_FIGHT2 && a->mode!=chase_player);
    assert(a->animframe==1);
    a->mode(a);
    unsigned flames=0;
    for(unsigned k=0;k<AVP_NUM_AMPS;++k)
        if(level1amps[k].creature==AC_FIRE && level1amps[k].mode==pre_flame_on)
            ++flames;
    assert(flames==4u);
    assert(discflag!=0);
    avp_amp_set_beam_weapon_sample(NULL);
    puts("Jaguar enemy Marine search, cloak detection, pulse/flame attack and reacquisition PASS");
    return 0;
}
