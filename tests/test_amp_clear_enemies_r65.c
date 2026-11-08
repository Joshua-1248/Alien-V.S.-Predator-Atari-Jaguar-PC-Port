/* R65: F12 Clear Enemies From Level frees hostile AMPs without any death
 * state or boss/campaign advancement. Non-hostile pickups and props survive. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>

s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos=0x00128000u,y_pos=0x00348000u,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy=90,player_type=PT_HUMAN,use_cocoon;
volatile u16 seed0,seed1;
extern unsigned amp_test_alarm_kills;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return ++seed0;}

static AvpAmp *make_actor(u16 type,unsigned x,unsigned y,unsigned flags)
{
    AvpAmp *a=amp_req();
    a->xpos=(s32)((x<<16)|0x8000u);
    a->ypos=(s32)((y<<16)|0x8000u);
    a->creature=type;
    a->energy=3000;
    a->oldenergy=3000;
    a->flags=(u16)flags;
    a->mode=chase_player;
    a->astype=0;
    amp_setgrid(a);
    return a;
}

int main(void)
{
    AvpAmp *alien,*queen,*generator,*egg,*hugger,*shield,*gas,*barrel,*player;
    unsigned alarm_before;
    InitAMPs();build_level();
    game_over=0;key_lock=0;score=37;player_energy=90;
    alien=make_actor(AC_ALIEN,10,10,1u<<AMP_KILLABLE);
    queen=make_actor(AC_AQUEEN,11,10,1u<<AMP_KILLABLE);
    generator=make_actor(AC_GEN,12,10,1u<<AMP_KILLABLE);
    egg=make_actor(AC_EGG,13,10,1u<<AMP_KILLABLE);
    hugger=make_actor(AC_FACEHUG,14,10,1u<<AMP_KILLABLE);
    shield=make_actor(AC_AQSHIELD,15,10,0);
    gas=make_actor(AC_GAS,16,10,0);
    barrel=make_actor(AC_DRUM,17,10,1u<<AMP_KILLABLE);
    player=make_actor(AC_ALIEN,18,10,1u<<AMP_PLAYER);
    ngens=1;
    /* Ensure stale collision ownership and object maps are scrubbed only
     * for removed hostiles; unrelated map entries and pickups remain. */
    collmap[20*AVP_AMP_GRID_W+20]=2;
    collmap[21*AVP_AMP_GRID_W+20]=0xffffu;
    objmap[(22*64u+20u)*2u+1u]=2u;
    objmap[(22*64u+21u)*2u+1u]=7u;
    alarm_before=amp_test_alarm_kills;
    assert(avp_amp_dev_clear_enemies()==6u);
    assert(!alien->mode && !queen->mode && !generator->mode);
    assert(!egg->mode && !hugger->mode && !shield->mode);
    assert(gas->mode && barrel->mode && player->mode);
    assert(collmap[10*AVP_AMP_GRID_W+10]==0u);
    assert(collmap[10*AVP_AMP_GRID_W+11]==0u);
    assert(collmap[10*AVP_AMP_GRID_W+16]==7u);
    assert(collmap[10*AVP_AMP_GRID_W+17]==8u);
    assert(collmap[10*AVP_AMP_GRID_W+18]==9u);
    assert(collmap[20*AVP_AMP_GRID_W+20]==0u);
    assert(collmap[21*AVP_AMP_GRID_W+20]==0xffffu);
    assert(objmap[(22*64u+20u)*2u+1u]==0u);
    assert(objmap[(22*64u+21u)*2u+1u]==7u);
    assert(game_over==0 && key_lock==0 && score==37 && player_energy==90);
    assert(ngens==0 && amp_test_alarm_kills==alarm_before+1u);
    assert(avp_amp_dev_clear_enemies()==0u);
    save_level();
    for(const AvpAmpSavedPlacement *p=avp_amp_saved_level(1);p->x!=-1;p++){
        assert(p->x!=(s16)((11u<<8)|0x80u));
        assert(p->x!=(s16)((12u<<8)|0x80u));
    }
    /* Reused freed slots must not duplicate a live actor's allocation. */
    AvpAmp *r1=amp_req(),*r2=amp_req();
    assert(r1!=r2 && r1!=gas && r2!=gas);
    puts("R65 clear enemies (Queen, generators, eggs, item safety) PASS");
    return 0;
}
