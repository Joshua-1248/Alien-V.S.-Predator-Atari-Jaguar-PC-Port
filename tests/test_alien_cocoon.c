/* End-to-end source-state regression for the Jaguar Alien Marine cocoon:
 * claw, tail-stun, claw, fallen-Marine contact, HUD incubation and respawn.
 * The actual collision and damage formulas are separately exercised in the
 * weapon/player tests; do not use diagnostic behavior in production. */
#include "amp.h"
#include "collision.h"
#include "hud.h"
#include "player.h"
#include "levels.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <string.h>

s16 cur_level=1,new_level=0;
s32 new_x=-1,new_y=-1;
s16 game_over,key_lock;
u8 acs_level;
u32 x_pos=0x00018000u,y_pos=0x00018000u;
u16 invisflag;
s16 player_energy=200,max_energy=200,player_type=PT_ALIEN;
uintptr_t savegame;
s32 score;
u32 fire_distance;
static unsigned messages[12],sounds[512],last_sound;
static u16 rnd_counter;
void place_grid(void){}
int LineOfSight(const AvpXY *from,const AvpXY *to){(void)from;(void)to;return 0;}
u16 avp_random(void){return rnd_counter++;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static void capture_message(void *u,unsigned id){(void)u;assert(id<12);messages[id]++;}
static void capture_sound(void *u,unsigned id){(void)u;last_sound=id;if(id<512)sounds[id]++;}
static void prepare_marine(AvpAmp *a){
    memset(a,0,sizeof(*a));
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->energy=a->oldenergy=50;a->xpos=0x00018000;a->ypos=0x00018000;
    a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    a->timer=1;a->astype=2;
}
static void down_marine(AvpAmp *a){
    /* The three original weapon events (partially charged in quick sequence):
     * claw reduces health, tail supplies STUN, follow-up claw brings it to zero.
     * Bite must never be included: it explicitly removes the stun bit. */
    a->energy-=15; /* claw, 50 -> 35 */
    a->energy-=12; /* tail, 35 -> 23 */
    a->flags|=(1u<<AMP_STUN);
    chase_player(a);
    assert(a->mode==stun_mode);
    assert(a->energy==23);
    stun_mode(a);
    assert(messages[7]==0);
    a->energy-=23; /* finishing claw, 23 -> 0 */
    stun_mode(a);
    assert(a->mode==stun_death);
    assert(messages[7]==1); /* COCOON THE ENEMY */
    assert(!(collmap[1u*AVP_AMP_GRID_W+1u]));
}
int main(void){
    AvpRuntimeOps ops={0};
    AvpAmp *a;
    ops.set_message=capture_message;ops.play_sfx=capture_sound;
    avp_runtime_bind(&ops);
    InitAMPs();build_level();InitCocoons();
    assert(num_cocoons==0);
    a=amp_req();prepare_marine(a);
    collmap[1u*AVP_AMP_GRID_W+1u]=1u;
    down_marine(a);
    /* Cannot instantly cocoon a body from a distance. */
    x_pos+=(60u<<9);
    for(int i=0;i<5;i++)stun_death(a);
    assert(a->mode==stun_death);
    assert(num_cocoons==0);
    x_pos=0x00018000u;
    stun_death(a);
    assert(a->mode==NULL&&a->host_static);
    assert(a->creature==AC_COCOON&&a->astype==18);
    assert(num_cocoons==1&&cocoon_data[2].frame==-1);
    assert(messages[8]==1&&messages[9]==0);
    assert(last_sound==AVP_FX_COCOX);
    assert(!CheckCocoon());
    for(int i=0;i<17*70+2;i++)UpdateCocoons();
    assert(cocoon_data[2].frame==17);
    assert(CheckCocoon());
    player_energy=0;new_level=0;UseCocoon();
    assert(new_level==1);
    assert(new_x==0x00018000&&new_y==0x00018000);
    assert(player_energy==max_energy);
    assert(num_cocoons==0&&!CheckCocoon());
    /* A ready cocoon created in another retail level must remain a valid
     * level-transfer resurrection rather than teleporting within this one. */
    MakeCocoon(0x00048000u,0x00058000u);
    assert(num_cocoons==1);
    cocoon_data[2].level=5;
    cocoon_data[2].frame=17;
    new_level=0;new_x=-1;new_y=-1;player_energy=0;
    assert(CheckCocoon());
    UseCocoon();
    assert(new_level==5&&new_x==0x00048000&&new_y==0x00058000);
    assert(num_cocoons==0&&player_energy==max_energy);
    /* At capacity the original branch reports failure, not a fourth cocoon. */
    memset(messages,0,sizeof(messages));
    num_cocoons=3;
    a=amp_req();prepare_marine(a);
    a->mode=stun_death;a->animframe=2;a->timer=5;
    stun_death(a);
    assert(messages[9]==1&&messages[8]==0);
    assert(num_cocoons==3 && a->host_static && a->creature==AC_HUMAN);
    return 0;
}
