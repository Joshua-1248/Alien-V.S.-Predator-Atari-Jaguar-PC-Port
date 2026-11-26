/* Player-driven Jaguar Alien cocoon sequence: real player hitscan/shot path,
 * weapon commands, Marine AMP state transitions, and cocoon lifecycle. */
#include "amp.h"
#include "player.h"
#include "joypad.h"
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

volatile u32 joy_edge=0xffffffffu,joy_cur=0xffffffffu;
void PredAvail(void){}
s16 cur_level=1,new_level=0;
s32 new_x=-1,new_y=-1;
u8 acs_level,lift_key;
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
int AMPCollisions(s32 x,s32 y){(void)x;(void)y;abort();}
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
/* RE40 R201: simulate actual Jaguar 10-Hz weapon scripts and fatigue.
 * The older real-hits fixture used immediate fire_wep() and manually refilled
 * the weapon before every shot, bypassing active_wep/recharge and falsely
 * causing 30+20 lethal damage before the final claw. */
static unsigned timed_hit(AvpAmp *a)
{
    const s16 before=a->energy;
    wep_fire=0xff;
    for(unsigned tick=0;tick<60u;tick++){
        UpdtScrOverlays();
        if(a->energy!=before){
            wep_fire=0;
            assert(a->energy<before);
            return (unsigned)(before-a->energy);
        }
    }
    assert(!"attack frame did not reach real TestSpark hit");
    return 0;
}
static void timed_select(s16 weapon)
{
    wep_fire=0;
    new_wepno=weapon;
    for(unsigned tick=0;tick<60u;tick++){
        UpdtScrOverlays();
        /* Have the script run its real source-authored equip animation. */
        if(cur_wepno==weapon && tick>15u)return;
    }
    assert(!"weapon switch did not complete");
}
static void test_full_health_with_real_attack_fatigue(void)
{
    AvpAmp *a;
    InitAMPs();build_level();InitCocoons();
    InitWeps();
    player_type=PT_ALIEN;
    cur_weps=0x0eu;
    cur_wepno=2;new_wepno=0;
    wep_fire=0;
    new_weapon();
    for(int i=0;i<16;i++)UpdtScrOverlays();
    a=amp_req();memset(a,0,sizeof *a);
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->energy=a->oldenergy=50;
    a->xpos=(s32)x_pos+0x4000;a->ypos=(s32)y_pos;
    a->astype=2;a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    unsigned claw1=timed_hit(a);
    assert(!(a->flags&(1u<<AMP_STUN)));
    timed_select(1);
    unsigned tail=timed_hit(a);
    assert(a->energy>0);
    assert(a->flags&(1u<<AMP_STUN));
    chase_player(a);
    assert(a->mode==stun_mode);
    timed_select(2);
    unsigned claw2=timed_hit(a);
    assert(a->energy<=0);
    stun_mode(a);
    assert(a->mode==stun_death);
    assert(messages[7]>=1);
    for(int i=0;i<3;i++)stun_death(a);
    x_pos=(u32)a->xpos;
    stun_death(a);
    assert(num_cocoons==1 && a->creature==AC_COCOON);
    printf("real 10Hz Claw/Tail/Claw: damage %u/%u/%u, Marine 50->%d, cocoon PASS\n",claw1,tail,claw2,(int)a->energy);
}
/* Full retail PlayerWeapons input routing: hold A, C, A to actual contact.
 * The newer host video mapping merely supplies active-low bits to this path. */
static int simulate_enemy=0;
static unsigned input_hit(AvpAmp *a,unsigned button)
{
    s16 before=a->energy;
    for(unsigned tick=0;tick<60;tick++){
        joy_cur=~(1u<<button);
        PlayerWeapons();
        if(simulate_enemy)UpdateAMPs();
        UpdtScrOverlays();
        if(a->energy!=before){
            joy_cur=0xffffffffu;
            assert(a->energy<before);
            return (unsigned)(before-a->energy);
        }
    }
    assert(!"retail button input failed to hit the Marine");
    return 0;
}
static void test_input_sequence(void)
{
    AvpAmp *a;
    InitAMPs();build_level();InitCocoons();
    player_type=PT_ALIEN;
    InitWeps();
    cur_weps=0x0eu;
    cur_wepno=2;new_wepno=0;
    wep_fire=0;new_weapon();
    for(unsigned i=0;i<16;i++){
        joy_cur=0xffffffffu;
        PlayerWeapons();UpdtScrOverlays();
    }
    a=amp_req();memset(a,0,sizeof *a);
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->energy=a->oldenergy=50;
    a->xpos=(s32)x_pos+0x4000;a->ypos=(s32)y_pos;
    a->astype=2;a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    simulate_enemy=1;
    unsigned c1=input_hit(a,FIRE_A);
    unsigned t=input_hit(a,FIRE_C);
    if(a->energy<=0){
        fprintf(stderr,"original A/C press damage %u+%u; premature death at energy=%d\n",c1,t,(int)a->energy);
    }
    assert(a->energy>0 && (a->flags&(1u<<AMP_STUN)));
    for(unsigned i=0;i<8u && a->mode!=stun_mode;i++)UpdateAMPs();
    assert(a->mode==stun_mode);
    unsigned c2=input_hit(a,FIRE_A);
    for(unsigned i=0;i<8u && a->mode!=stun_death;i++)UpdateAMPs();
    assert(a->mode==stun_death);
    simulate_enemy=0;
    for(int i=0;i<3;i++)stun_death(a);
    x_pos=(u32)a->xpos;stun_death(a);
    assert(num_cocoons==1 && a->creature==AC_COCOON);
    printf("real A/C/A input: damage %u/%u/%u, cocoon PASS\n",c1,t,c2);
}
static void test_artificial_full_charge_and_alternate_order(void)
{
    AvpAmp *a;
    InitAMPs();build_level();InitCocoons();
    player_type=PT_ALIEN;
    InitWeps();
    a=amp_req();memset(a,0,sizeof *a);
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->energy=a->oldenergy=50;
    a->xpos=(s32)x_pos+0x4000;a->ypos=(s32)y_pos;
    a->astype=2;a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    /* The old test forcibly refilled each weapon and skipped its authored
     * attack frames. Its artificial Claw+Tail=30+20 fatality is accurately
     * handled by unmodified Jaguar source; this is NOT the real combo. */
    hit_as(2);assert(a->energy==20);
    hit_as(1);assert(a->energy==0 && (a->flags&(1u<<AMP_STUN)));
    chase_player(a);
    assert(a->mode==chase_player && !(a->flags&(1u<<AMP_STUN)));
    assert(num_cocoons==0);
    InitAMPs();build_level();InitCocoons();
    /* Tail first can stun, then Claw can finish, but the documented combo
     * is A-C-A. Do not introduce a separate host combo state machine. */
    a=amp_req();memset(a,0,sizeof *a);
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->energy=a->oldenergy=50;
    a->xpos=(s32)x_pos+0x4000;a->ypos=(s32)y_pos;
    a->astype=2;a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    hit_as(1);assert(a->energy==30 && (a->flags&(1u<<AMP_STUN)));
    chase_player(a);assert(a->mode==stun_mode);
    hit_as(2);assert(a->energy==0 && (a->flags&(1u<<AMP_STUN)));
    stun_mode(a);assert(a->mode==stun_death);
    InitAMPs();build_level();InitCocoons();
    /* The original bite explicitly cancels stun eligibility. */
    a=amp_req();memset(a,0,sizeof *a);
    a->mode=chase_player;a->creature=AC_HUMAN;
    a->energy=a->oldenergy=150;
    a->xpos=(s32)x_pos+0x4000;a->ypos=(s32)y_pos;
    a->astype=2;a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);
    hit_as(1);assert(a->flags&(1u<<AMP_STUN));
    hit_as(3);assert(!(a->flags&(1u<<AMP_STUN)));
    puts("full-charge shortcut, Tail-Claw and Bite-clear retail rules PASS");
}
int main(void){
    AvpRuntimeOps ops={0}; AvpAmp *a;
    ops.set_message=msg;ops.play_sfx=sound;avp_runtime_bind(&ops);
    player_type=PT_ALIEN; player_energy=1000;max_energy=1000;
    test_full_health_with_real_attack_fatigue();
    x_pos=0x00018000u;y_pos=0x00018000u;
    test_input_sequence();
    x_pos=0x00018000u;y_pos=0x00018000u;
    test_artificial_full_charge_and_alternate_order();
    x_pos=0x00018000u;y_pos=0x00018000u;
    memset(messages,0,sizeof messages);
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
