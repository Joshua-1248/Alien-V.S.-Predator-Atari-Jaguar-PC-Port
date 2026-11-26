/* RE39 R188 — original 68000 ADDQ.L/SUBQ.L in seven live enemy/Queen
 * mode transitions. Include the actual production C translation unit so the
 * test reaches its static state handlers, not a copied arithmetic formula. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/game/amp.c"

u32 x_pos,y_pos,fire_distance,ccn_xsave,ccn_ysave;
s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u16 invisflag;
s32 score;
s16 player_type,player_energy,use_cocoon;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u16 avp_random(void){return 1;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}

static AvpAmp *ready(unsigned creature)
{
    InitAMPs();build_level();
    AvpAmp *a=amp_req();
    assert(a==&level1amps[0]);
    a->creature=(u16)creature;
    a->energy=a->oldenergy=100;
    a->xpos=a->ypos=0x000a8000;
    a->xvel=a->yvel=0x100;
    a->animseq=AS_RUN;
    a->mode=chase_player;
    a->flags=0;
    x_pos=y_pos=0x001a8000;
    player_type=PT_HUMAN;player_energy=300;
    invisflag=discflag=0;
    host_attacks_disabled=0;
    return a;
}

static void detour_and_hidden(int boundary)
{
    AvpAmp *a=ready(AC_ALIEN);
    a->mode=amp_fake_north;
    a->yvector=boundary?INT32_MIN:2;
    amp_fake_north(a);
    assert(a->yvector==(boundary?INT32_MAX:1));
    assert(a->mode==amp_fake_north);
    if(!boundary){a->mode(a);assert(a->yvector==0 && a->mode==chase_player);}
    a=ready(AC_PREDATOR);
    a->animseq=AS_HIDDEN;
    a->mode=amp_pred_hold;
    a->yvector=boundary?INT32_MIN:2;
    amp_pred_hold(a);
    assert(a->yvector==(boundary?INT32_MAX:1));
    if(!boundary){amp_pred_hold(a);assert(a->yvector==0 && a->mode==chase_player);}
}

static void melee(int boundary)
{
    AvpAmp *a=ready(AC_ALIEN);
    x_pos=y_pos=0x000a8000;
    a->mode=amp_handalien;
    a->timer=boundary?INT32_MAX:3;
    amp_handalien(a);
    assert(a->timer==(boundary?INT32_MIN:4));
    assert(a->mode==amp_handalien);
    if(!boundary){amp_handalien(a);assert(a->timer==0 && a->mode==amp_alien_clfight);}
    a=ready(AC_PREDATOR);
    x_pos=y_pos=0x000a8000;
    a->mode=amp_handpred;
    a->timer=boundary?INT32_MAX:3;
    amp_handpred(a);
    assert(a->timer==(boundary?INT32_MIN:4));
    assert(a->mode==amp_handpred);
    if(!boundary){amp_handpred(a);assert(a->timer==0 && a->mode==amp_pred_clfight);}
}

static void queen_states(int boundary)
{
    AvpAmp *a=ready(AC_AQUEEN);
    a->mode=queen_delay_mode;
    a->timer=boundary?INT32_MIN:2;
    queen_delay_mode(a);
    assert(a->timer==(boundary?INT32_MAX:1));
    if(!boundary){queen_delay_mode(a);assert(a->mode==queen_lock_mode && a->timer==3);}

    a=ready(AC_AQUEEN);
    ngens=1;
    a->mode=end_queen_mode;
    a->timer=boundary?INT32_MIN:2;
    end_queen_mode(a);
    assert(a->timer==(boundary?INT32_MAX:1));
    assert(a->mode==end_queen_mode);
    if(!boundary){end_queen_mode(a);assert(a->timer==0 && a->mode==end_queen_mode);}

    a=ready(AC_AQUEEN);
    a->mode=queen_lock_mode;
    a->timer=boundary?INT32_MIN:2;
    queen_choose_fight(a);
    assert(a->timer==(boundary?INT32_MAX:1));
    assert(a->mode==queen_lock_mode);
    if(!boundary){queen_choose_fight(a);assert(a->timer==0 && a->mode==queen_lock_mode);}
}

int main(void)
{
    for(int boundary=0;boundary<=1;boundary++){
        detour_and_hidden(boundary);
        melee(boundary);
        queen_states(boundary);
    }
    puts("R188 seven original AMP longword state paths, normal/boundary: PASS");
    return 0;
}
