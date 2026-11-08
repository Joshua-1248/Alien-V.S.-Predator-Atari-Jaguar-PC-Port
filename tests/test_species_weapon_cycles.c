/* Original Alien and Predator command streams: attack inputs, first-person
 * overlay states, melee damage, projectile kind and Jaguar sound identity.
 * This is a standard gameplay regression, not a diagnostic build patch. */
#include "mazescrn.h"
#include "collectables.h"
#include "player.h"
#include "weapons.h"
#include "avp_runtime.h"
#include "maze.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

s16 player_type=PT_ALIEN;
u16 player_vel=0;
u8 wep_fire=0;
s16 cheat=0,medpak=0,player_energy=3000,max_energy=3000;
u16 invisflag=0;
u8 invis_act=0,invis_stat=0,medpak_act=0;
AvpAmmoInfo ammo_info[6];
s32 score=0,sin_ang=0,cos_ang=0x7fff;
u8 cur_weps=0,old_weps=0;
s16 cur_wepno=0,new_wepno=0,alien_bite=0;
volatile u32 joy_edge=0xffffffffu;
uintptr_t savegame=0;
static unsigned sfx[64],nfx,melee_hits,projectiles;
static int hit_enabled=1;
static u16 projectile_kind;
static s16 hit_damage;
static u32 hit_dist;
static u16 hit_width;
static AvpAmp target;
s32 cos_d0(u16 angle){(void)angle;return 0x7fff;}
u32 TFX(unsigned id){if(nfx<64)sfx[nfx++]=id;return 0;}
/* R272: MAZESCRN.S wep_fx plays weapon sounds via `wep_sfx` = PlacedEf voice 1. */
static unsigned placed_voice1;
u32 avp_music_placed_voice(unsigned id,unsigned v){if(v==1u)++placed_voice1;return TFX(id);}
void StopLoop(u32 id){(void)id;}
AvpAmp *TestSpark(void){return NULL;}
AvpAmp *player_weapon(s32 hx,s32 hy,u16 kind,u16 reset,s32 sn,s32 cs){
    (void)hx;(void)hy;(void)reset;(void)sn;(void)cs;
    projectile_kind=kind;projectiles++;return NULL;
}
static AvpAmp *on_hit(s16 dmg,u32 dist,u16 width){
    melee_hits++;hit_damage=dmg;hit_dist=dist;hit_width=width;
    return hit_enabled?&target:NULL;
}
static void tick(void){update_wep();process_shot();recharge();}
static void attack(s16 species,s16 weapon,unsigned sound,u16 projectile,unsigned base_damage,unsigned hit_sound){
    AvpFirstPersonOverlayState st;
    const AvpWeaponDef *def=avp_weapon_def(species,weapon);
    assert(def && def->actions[0] && def->action_count==1);
    player_type=species;cur_wepno=weapon;new_wepno=0;wep_fire=0;
    cur_weps=(u8)(1u<<weapon);nfx=melee_hits=projectiles=0;
    hit_damage=0;projectile_kind=0xffff;
    InitWeps();new_weapon();
    for(int i=0;i<24;i++)tick();
    avp_mazescrn_get_overlay_state(&st);
    assert(st.overlay_id>=0 || weapon==2);
    wep_fire=0xff;
    for(int i=0;i<25;i++)tick();
    assert(nfx>=1);
    /* R272: MOTOR is emitted by fade_selects (MAZESCRN.S:3306-3314) when the
     * cannon icon is selected, not by new_weapon; see the check in main(). */
    assert(sfx[0]==sound);
    if(base_damage){
        assert(melee_hits>=1);
        assert(hit_damage>0 && hit_damage<=(s16)base_damage);
        assert(hit_dist==(u32)0x90u<<8 && hit_width==20u);
        assert(projectiles==0);
        /* Unlike R112, assert that hitting emitted the damage-event sound,
         * not just the first-person weapon's attack-start effect. */
        {unsigned found=0;for(unsigned i=1;i<nfx;i++)if(sfx[i]==hit_sound)found++;
         assert(found>=1u);}
    }else{
        assert(projectiles>=1 && projectile_kind==projectile);
        assert(melee_hits==0);
    }
    wep_fire=0;
    for(int i=0;i<25;i++)tick();
}
/* Verify the real Jaguar-script frame0/move-in fixed-point positions,
 * particularly negative tail offsets.  Assertions intentionally concern
 * CPU weapon state, not unproven Jaguar Object Processor display timing. */
static void check_alien_intro_positions(void){
    AvpFirstPersonOverlayState st;
    player_type=PT_ALIEN;cur_weps=0x0eu;new_wepno=0;wep_fire=0;
    InitWeps();
    cur_wepno=1;new_weapon();
    avp_mazescrn_get_overlay_state(&st);
    assert(st.x==(20L*65536L) && st.y==(-30L*65536L));
    tick();avp_mazescrn_get_overlay_state(&st);
    assert(st.x==(50L*65536L) && st.y==(-60L*65536L));
    tick();avp_mazescrn_get_overlay_state(&st);
    assert(st.x==(20L*65536L) && st.y==(-30L*65536L));
    cur_wepno=2;new_weapon();
    avp_mazescrn_get_overlay_state(&st);
    assert(st.x==(24L*65536L) && st.y==(72L*65536L));
    tick();avp_mazescrn_get_overlay_state(&st);
    assert(st.x==(24L*65536L) && st.y==(132L*65536L));
    tick();avp_mazescrn_get_overlay_state(&st);
    assert(st.y==(102L*65536L));
    cur_wepno=3;new_weapon();
    avp_mazescrn_get_overlay_state(&st);
    assert(st.x==0 && st.y==0);
    tick();avp_mazescrn_get_overlay_state(&st);
    assert(st.x==0 && st.y==(60L*65536L));
    tick();avp_mazescrn_get_overlay_state(&st);
    assert(st.y==(30L*65536L));
}
int main(void){
    avp_mazescrn_set_shot_callback(on_hit);
    check_alien_intro_positions();
    /* Jaguar Alien A/B/C are swipe, bite, tail, not interchangeable attacks. */
    attack(PT_ALIEN,2,AVP_FX_WHOOS,0,30,AVP_FX_CLAW2);
    attack(PT_ALIEN,3,AVP_FX_SWIPE,0,100,AVP_FX_CLAW2);
    attack(PT_ALIEN,1,AVP_FX_TAIL,0,20,AVP_FX_CLAW2);
    /* Predator honour weapons: melee stick, bolt, disc, default wrist blade. */
    attack(PT_PREDATOR,1,AVP_FX_MELEE,0,100,AVP_FX_MELEE);
    attack(PT_PREDATOR,2,AVP_FX_ROCKE,2,0,0);
    attack(PT_PREDATOR,3,AVP_FX_DISK1,0,0,0);
    attack(PT_PREDATOR,4,AVP_FX_WRBLA,0,30,AVP_FX_CLAW2);
    /* Missed melee blows retain their swing/attack sound but never emit a
     * contact cue.  This specifically catches unconditional impact patches. */
    hit_enabled=0;
    player_type=PT_ALIEN;cur_wepno=2;new_wepno=0;wep_fire=0;
    cur_weps=(u8)(1u<<2);nfx=melee_hits=projectiles=0;
    InitWeps();new_weapon();for(int i=0;i<24;i++)tick();
    wep_fire=0xff;for(int i=0;i<25;i++)tick();
    assert(melee_hits>=1);
    for(unsigned i=0;i<nfx;i++)assert(sfx[i]!=AVP_FX_CLAW2);
    /* Marine cocoon trigger uses only a successful Alien tail strike to
     * toggle AMP_STUN; claw preserves it and bite cancels eligibility.
     * This is the exact original Jaguar hit-flag branch, not a new combo. */
    hit_enabled=1;
    player_type=PT_ALIEN;wep_fire=0xff;
    target.flags=0;
    InitWeps();
    cur_wepno=2;new_weapon();fire_wep();process_shot();
    assert(!(target.flags&(1u<<AMP_STUN)));
    cur_wepno=1;new_weapon();fire_wep();process_shot();
    assert(target.flags&(1u<<AMP_STUN));
    cur_wepno=2;new_weapon();fire_wep();process_shot();
    assert(target.flags&(1u<<AMP_STUN));
    cur_wepno=3;new_weapon();fire_wep();process_shot();
    assert(!(target.flags&(1u<<AMP_STUN)));
    assert(placed_voice1>0u); /* weapon SOUND commands used voice 1 */
    /* R272: selecting the Predator cannon icon plays `wep_sfx motor1`. */
    player_type=PT_PREDATOR;cur_weps=(u8)((1u<<2)|(1u<<4));cur_wepno=4;new_wepno=2;
    init_fades();nfx=0;placed_voice1=0;
    fade_selects();
    assert(nfx==1u&&sfx[0]==AVP_FX_MOTOR&&placed_voice1==1u);
    nfx=0;fade_selects();assert(nfx==0u); /* only on the icon change */
    assert(placed_voice1>0u);
    return 0;
}
