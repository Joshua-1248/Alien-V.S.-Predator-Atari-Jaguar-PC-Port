/* RE39 R187: 68000 longword countdowns in actual AMP enemy modes.
 * Recoil recovery, live Marine stun, and stunned-body decay are production
 * routines.  R186 signed --timer invokes UB for the $80000000 boundary. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

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
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}

static AvpAmp *prepare(unsigned creature,unsigned flags,s16 energy,s16 oldenergy)
{
    InitAMPs();build_level();
    AvpAmp *a=amp_req();
    assert(a==&level1amps[0]);
    a->mode=chase_player;
    a->creature=(u16)creature;
    a->flags=(u16)flags;
    a->energy=energy;
    a->oldenergy=oldenergy;
    a->animseq=AS_RUN;
    a->xvel=a->yvel=1;
    a->xpos=0x000a8000;
    a->ypos=0x000a8000;
    x_pos=0x000a8000u;
    y_pos=0x000a8000u;
    player_type=PT_ALIEN;
    player_energy=300;
    invisflag=discflag=0;
    return a;
}
static void test_recoil(int boundary)
{
    AvpAmp *a=prepare(AC_ALIEN,0u,90,100);
    chase_player(a);
    assert(a->mode && a->mode!=chase_player);
    assert(a->timer==2 && a->animseq==AS_KNOCKBACK);
    if(boundary){
        a->timer=INT32_MIN;
        AvpAmpModeFn f=a->mode;
        f(a);
        assert(a->timer==INT32_MAX && a->mode==f);
    }else{
        a->mode(a);
        assert(a->timer==1 && a->mode!=chase_player);
        a->mode(a);
        assert(a->timer==0 && a->mode==chase_player && a->animseq==AS_RUN);
    }
}
static void test_stun(int boundary)
{
    AvpAmp *a=prepare(AC_HUMAN,1u<<AMP_STUN,25,25);
    chase_player(a);
    assert(a->mode==stun_mode && a->timer==25);
    if(boundary){
        a->timer=INT32_MIN;
        stun_mode(a);
        assert(a->timer==INT32_MAX && a->mode==stun_mode);
        assert(a->flags&(1u<<AMP_STUN));
    }else{
        a->timer=2;
        stun_mode(a);
        assert(a->timer==1 && a->mode==stun_mode);
        stun_mode(a);
        assert(a->timer==0 && a->mode==stun_mode);
        stun_mode(a);
        assert(a->mode==chase_player && !(a->flags&(1u<<AMP_STUN)));
    }
}
static void test_stun_death(int boundary)
{
    AvpAmp *a=prepare(AC_HUMAN,1u<<AMP_STUN,25,25);
    a->mode=stun_death;
    a->animseq=AS_DEATH;
    a->animframe=2;
    a->timer=boundary?INT32_MIN:2;
    /* No player contact, so no host-only cocoon transition is involved. */
    x_pos=0x00208000u;y_pos=0x00208000u;
    stun_death(a);
    if(boundary){
        assert(a->timer==INT32_MAX && a->mode==stun_death && !a->host_static);
    }else{
        assert(a->timer==1 && a->mode==stun_death);
        stun_death(a);
        assert(a->timer==0 && a->mode==NULL && a->host_static);
    }
}
int main(int argc,char **argv)
{
    (void)argv;
    for(int boundary=0;boundary<=1;boundary++){
        test_recoil(boundary);
        test_stun(boundary);
        test_stun_death(boundary);
    }
    assert(argc==1);
    puts("R187 AMP recoil / Marine stun / stunned-body 68000 countdowns: 6 modes PASS");
    return 0;
}
