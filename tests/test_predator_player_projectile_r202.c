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
#include <string.h>
#include <stdio.h>
#include "avp_sfx_ids.h"

volatile u32 joy_edge=0xffffffffu,joy_cur=0xffffffffu;
void PredAvail(void){}
s16 cur_level=1,new_level=0;
s32 new_x=-1,new_y=-1;
u8 acs_level,lift_key;
int AMPCollisions(s32 px,s32 py){(void)px;(void)py;return 0;}
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
static unsigned player_audio[64], player_audio_count, placed_count, placed_voice_bad;
static unsigned npc_audio[64], npc_audio_count;
u32 TFX(unsigned id){if(player_audio_count<64u)player_audio[player_audio_count]=id; ++player_audio_count;return player_audio_count;}
u32 avp_music_placed_voice(unsigned id,unsigned v){if(v!=1u)++placed_voice_bad; ++placed_count;return TFX(id);}
static void record_npc(void *user,unsigned id){(void)user; if(npc_audio_count<64u)npc_audio[npc_audio_count]=id; ++npc_audio_count;}
static unsigned count_id(unsigned id){unsigned n=0;for(unsigned i=0;i<player_audio_count && i<64u;i++)if(player_audio[i]==id)++n;return n;}


int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}


/* R202 reimplementation audit: execute the *real player* Predator weapon
 * interpreter and the *real* AMP projectile allocator together. R201 changed
 * only an enemy AMP attack-state cue; this guards against accidentally
 * removing the separately authored player cue or projectile launch. */
static void setup_predator(s16 weapon)
{
    player_type=PT_PREDATOR;
    player_energy=max_energy=2500;
    invisflag=0;invis_act=0;invis_stat=0;
    joy_cur=joy_edge=0xffffffffu;
    x_pos=y_pos=0x00018000u;
    sin_ang=0;cos_ang=0x7fff;
    InitAMPs();build_level();InitCocoons();
    InitWeps();
    cur_weps=(u8)((1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6));
    cur_wepno=weapon;new_wepno=0;
    wep_fire=0;new_weapon();
    player_audio_count=placed_count=placed_voice_bad=npc_audio_count=0;
    memset(player_audio,0,sizeof player_audio);
    memset(npc_audio,0,sizeof npc_audio);
    for(unsigned i=0;i<18u;i++){
        PlayerWeapons();UpdtScrOverlays();
    }
    assert(player_audio_count==0u && npc_audio_count==0u);
}
static AvpAmp *find_player_projectile(u16 creature)
{
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        AvpAmp *a=&amp_data[i];
        if(a->creature==creature && (a->flags&(1u<<AMP_PLAYER)) && a->mode)
            return a;
    }
    return NULL;
}
static void exercise_predator_projectile(s16 weapon,u16 creature,unsigned cue)
{
    setup_predator(weapon);
    AvpAmp *a=NULL;
    for(unsigned i=0;i<35u;i++){
        joy_cur=~(1u<<FIRE_A);
        joy_edge=(i==0u)?joy_cur:0xffffffffu;
        PlayerWeapons();UpdtScrOverlays();
        a=find_player_projectile(creature);
        if(a)break;
    }
    assert(a!=NULL);
    assert(count_id(cue)==1u);       /* exactly one player attack cue */
    assert(placed_count==1u);        /* original reserved weapon voice */
    assert(placed_voice_bad==0u);    /* voice 1, not enemy AMP mixer */
    assert(npc_audio_count==0u);     /* no enemy attack audio side effect */
    assert(a->xvel==-1);            /* player-weapon launch, not NPC launch */
    assert(a->flags&(1u<<AMP_PLAYER));
    assert(a->flags&(1u<<AMP_PHIT));
    assert(a->yvel==0);
    assert(a->xvector>0);
    assert(a->creature==creature);
    joy_cur=joy_edge=0xffffffffu;
    /* Completing the release animation must not emit the missing enemy cue. */
    for(unsigned i=0;i<13u;i++){PlayerWeapons();UpdtScrOverlays();}
    assert(count_id(cue)==1u);
    assert(npc_audio_count==0u);
    printf("Predator player weapon %d: cue %u once, AMP projectile %d, release PASS\n",
           (int)weapon,cue,(int)creature);
}
int main(void)
{
    AvpRuntimeOps ops={0};
    ops.play_sfx=record_npc;
    avp_runtime_bind(&ops);
    /* R201 enemy DISK1 suppression must not silence the player's DISK1 or
     * move the exact attack-frame launch of the Smart Disc. */
    assert(avp_weapon_def(PT_PREDATOR,3)!=NULL);
    assert(!strcmp(avp_weapon_def(PT_PREDATOR,3)->name,"smart disc"));
    exercise_predator_projectile(3,AC_SMART,AVP_FX_DISK1);
    /* The same player command interpreter must still fire the original
     * Shoulder Cannon bolt and play its distinct ROCKET sound. */
    assert(!strcmp(avp_weapon_def(PT_PREDATOR,2)->name,"shoulder cannon"));
    exercise_predator_projectile(2,AC_BOLT,AVP_FX_ROCKE);
    avp_runtime_reset_ops();
    puts("R202 Predator player Smart Disc / cannon audio isolation PASS");
    return 0;
}
