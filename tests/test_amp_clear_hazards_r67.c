/* R67: targeted direct hazard despawn, with Queen and all pickups preserved. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>

s16 cur_level=15,num_cocoons,game_over,key_lock;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos=0x00128000u,y_pos=0x00348000u,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy=93,player_type=PT_HUMAN,use_cocoon;
volatile u16 seed0,seed1;
extern unsigned amp_test_alarm_kills;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return ++seed0;}

static AvpAmp *make_actor(u16 type,unsigned x,unsigned y,unsigned flags,s16 def)
{
    AvpAmp *a=amp_req();
    a->xpos=(s32)((x<<16)|0x8000u);
    a->ypos=(s32)((y<<16)|0x8000u);
    a->creature=type;
    a->energy=100;
    a->oldenergy=100;
    a->flags=(u16)flags;
    a->mode=chase_player;
    a->astype=def;
    amp_setgrid(a);
    return a;
}

int main(void)
{
    AvpAmp *queen,*alien,*player,*gas,*gas_alias,*aid,*drum,*shield,*gen,*acid,*corpse;
    unsigned alarm_before;
    InitAMPs();build_level();
    game_over=0;key_lock=0;score=91;player_energy=93;
    queen=make_actor(AC_AQUEEN,10,10,1u<<AMP_KILLABLE,50);
    alien=make_actor(AC_ALIEN,11,10,1u<<AMP_KILLABLE,0);
    gas=make_actor(AC_GAS,12,10,0,22);
    aid=make_actor(AC_AID,13,10,0,11);
    drum=make_actor(AC_DRUM,14,10,1u<<AMP_KILLABLE,20);
    shield=make_actor(AC_AQSHIELD,15,10,0,49);
    gen=make_actor(AC_GEN,16,10,1u<<AMP_KILLABLE,44);
    acid=make_actor(AC_ALIEN,17,10,0,52);
    acid->mode=avp_amp_builtin_mode_for_def(52);
    assert(avp_amp_is_acid_puddle(acid));
    corpse=make_actor(AC_ALIEN,18,10,0,52);
    /* A still-dying Alien is NOT an acid puddle. */
    assert(!avp_amp_is_acid_puddle(corpse));
    player=make_actor(AC_HUMAN,19,10,1u<<AMP_PLAYER,2);
    gas_alias=make_actor(AC_DRUM,21,10,0,22); /* retail alias, not DRUM template 20 */
    gen->aux_ptr=shield;
    ngens=4;
    /* Deliberately stale grid ownership, and a protected map sentinel. */
    collmap[20*AVP_AMP_GRID_W+20]=5;
    collmap[21*AVP_AMP_GRID_W+20]=0xffffu;
    objmap[(22*64u+20u)*2u+1u]=6u;
    objmap[(22*64u+21u)*2u+1u]=3u;
    alarm_before=amp_test_alarm_kills;
    assert(avp_amp_dev_clear_hazards()==4u);
    assert(!drum->mode && !shield->mode && !gen->mode && !acid->mode);
    assert(queen->mode && alien->mode && player->mode && gas->mode && gas_alias->mode && aid->mode && corpse->mode);
    assert(collmap[10*AVP_AMP_GRID_W+10]==1u);
    assert(collmap[10*AVP_AMP_GRID_W+12]==3u);
    assert(collmap[10*AVP_AMP_GRID_W+14]==0u);
    assert(collmap[10*AVP_AMP_GRID_W+15]==0u);
    assert(collmap[10*AVP_AMP_GRID_W+16]==0u);
    assert(collmap[10*AVP_AMP_GRID_W+17]==0u);
    assert(collmap[20*AVP_AMP_GRID_W+20]==0u);
    assert(collmap[21*AVP_AMP_GRID_W+20]==0xffffu);
    assert(objmap[(22*64u+20u)*2u+1u]==0u);
    assert(objmap[(22*64u+21u)*2u+1u]==3u);
    assert(game_over==0 && key_lock==0 && score==91 && player_energy==93);
    assert(ngens==0 && amp_test_alarm_kills==alarm_before+1u);
    assert(avp_amp_dev_clear_hazards()==0u);
    save_level();
    for(const AvpAmpSavedPlacement *p=avp_amp_saved_level(15);p->x!=-1;p++){
        assert(p->x!=(s16)((14u<<8)|0x80u));
        assert(p->x!=(s16)((15u<<8)|0x80u));
        assert(p->x!=(s16)((16u<<8)|0x80u));
        assert(p->x!=(s16)((17u<<8)|0x80u));
    }
    assert(amp_req()!=gas);
    puts("R67 clear hazards (drums, Queen shields/generators, acid, protected items/Queen) PASS");
    return 0;
}
