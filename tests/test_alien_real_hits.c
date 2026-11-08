/* Player-driven Jaguar Alien cocoon sequence: real player hitscan/shot path,
 * weapon commands, Marine AMP state transitions, and cocoon lifecycle. */
#include "amp.h"
#include "player.h"
#include "mazescrn.h"
#include "weapons.h"
#include "collectables.h"
#include "hud.h"
#include "collision.h"
#include "levels.h"
#include "avp_runtime.h"
#include "savegame.h"
#include "eeprom.h"
#include <assert.h>
#include <string.h>

s16 cur_level=1,new_level=0;
s32 new_x=-1,new_y=-1;
u8 acs_level;
u32 x_pos=0x00018000u,y_pos=0x00018000u,centre_angle=0;
u32 FireDistance(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x7fff;}
void place_grid(void){}
AvpAmmoInfo ammo_info[6];
u8 cur_weps; s16 cur_wepno,new_wepno,medpak;
s16 alien_bite; s16 show_mt;
s32 sin_ang=0,cos_ang=0x7fff;
u32 fire_angle=0;
void Vector(const AvpXY *from,const AvpXY *to,s32 *dx,s32 *dy){*dx=to->x-from->x;*dy=to->y-from->y;}
void AreaDamage(s32 x,s32 y,s32 radius,s16 damage,u16 flags){(void)x;(void)y;(void)radius;(void)damage;(void)flags;}
/* AMP's explosion path has a separate area-damage entry in the original
 * collision module. This Alien melee/cocoon fixture never calls that path;
 * supply its unused external so the actual AMP object links unchanged. */
void AreaAmpDamage(s32 x,s32 y,s32 radius,s16 damage,u16 flags){(void)x;(void)y;(void)radius;(void)damage;(void)flags;}
u8 SafePos(u8 h,s32 x,s32 y,u16 w){(void)h;(void)x;(void)y;(void)w;return AVP_COLL_SAFE;}
int avp_doors_cell_is_liftside(s32 x,s32 y){(void)x;(void)y;return 0;}
u32 DoEffect(unsigned id){(void)id;return 1;}
void StopLoop(u32 h){(void)h;}
void StartAla(unsigned id){(void)id;}
void KillAlar(void){}
int avp_collision_open_door_ahead(s32 x,s32 y,int dx,int dy,u8 h){(void)x;(void)y;(void)dx;(void)dy;(void)h;return 0;}
u8 *avp_collision_maze_data(void){return NULL;}
u16 avp_collision_maze_width(void){return 0;}
u16 avp_collision_maze_height(void){return 0;}
u32 TFX(unsigned id){(void)id;return 1;}
u32 avp_music_placed_voice(unsigned id,unsigned v){(void)v;return TFX(id);}

int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}

static int messages[12];
static void msg(void *u,unsigned n){(void)u;assert(n<12);++messages[n];}
static void sound(void *u,unsigned n){(void)u;(void)n;}
static void hit_as(s16 wep){
    cur_wepno=wep;new_wepno=0;
    new_weapon();
    ammo_info[wep-1].cur=AVP_APAMM_MAX;
    wep_fire=0xff;
    fire_wep();
    process_shot();
}
int main(void){
    AvpRuntimeOps ops={0}; AvpAmp *a;
    ops.set_message=msg;ops.play_sfx=sound;avp_runtime_bind(&ops);
    player_type=PT_ALIEN; player_energy=1000;max_energy=1000;
    InitAMPs();build_level();InitCocoons();InitWeps();
    a=amp_req(); memset(a,0,sizeof(*a));a->mode=chase_player;
    a->creature=AC_HUMAN;a->energy=a->oldenergy=55;
    a->xpos=(s32)x_pos+0x4000;a->ypos=(s32)y_pos;a->astype=2;
    a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    hit_as(2);assert(a->energy==25 && !(a->flags&(1u<<AMP_STUN)));
    hit_as(1);assert(a->energy==5 && (a->flags&(1u<<AMP_STUN)));
    chase_player(a);assert(a->mode==stun_mode);
    hit_as(2);assert(a->energy==-25 && (a->flags&(1u<<AMP_STUN)));
    stun_mode(a);assert(a->mode==stun_death);assert(messages[7]==1);
    for(int i=0;i<3;i++)stun_death(a);
    assert(!num_cocoons);
    x_pos=(u32)a->xpos;stun_death(a);
    assert(num_cocoons==1 && messages[8]==1 && a->creature==AC_COCOON);
    for(int i=0;i<17*70+2;i++)UpdateCocoons();
    assert(CheckCocoon());
    MakeSave(-1);
    savegame=(uintptr_t)SaveCont;
    InitCocoons();
    assert(num_cocoons==1 && CheckCocoon());
    UpdateCocoons();
    assert(CheckCocoon());
    player_energy=0;
    UseCocoon();
    assert(player_energy==max_energy && new_level==cur_level);
    assert(new_x==(s32)(((u32)a->xpos&0xffff0000u)|0x8000u));
    assert(!num_cocoons && !CheckCocoon());

    /* With two cocoons, the oldest occupies slot 2 and can revive the
     * player even while the newly created second cocoon is immature.
     * Saving/reloading must preserve that order and the correct coordinates. */
    savegame=0;
    MakeCocoon(0x00068000u,0x00078000u);
    for(int i=0;i<17*70+2;i++)UpdateCocoons();
    assert(CheckCocoon());
    MakeCocoon(0x00088000u,0x00098000u);
    assert(num_cocoons==2 && CheckCocoon());
    MakeSave(-1);
    savegame=(uintptr_t)SaveCont;
    InitCocoons();
    assert(num_cocoons==2 && CheckCocoon());
    assert(cocoon_data[2].x==0x00068000u);
    assert(cocoon_data[1].x==0x00088000u);
    player_energy=0;
    UseCocoon();
    assert(player_energy==max_energy && new_x==0x00068000);
    assert(num_cocoons==1 && !CheckCocoon());
    return 0;
}
