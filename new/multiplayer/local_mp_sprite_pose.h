#ifndef AVP_LOCAL_MP_SPRITE_POSE_H
#define AVP_LOCAL_MP_SPRITE_POSE_H
/* Original AMP.S sprite frames, reused for render-only co-op player avatars.
 * Never alters the actual PLAYER/WEAPONS attack cadence or damage timing. */
#include "amp.h"
#include <stdint.h>

typedef struct AvpMpSpritePose {uint16_t sequence,frame;unsigned active;} AvpMpSpritePose;

/* The original enemy-derived visual frame lists are retained. Marine front
 * presentation uses a separate override below; actual weapon attacks are
 * still handled solely by the native MAZESCRN.S player simulation. */
static inline AvpMpSpritePose avp_mp_sprite_attack_pose(unsigned species,
    unsigned weapon,unsigned age)
{
    static const uint8_t bite[]={0,1,2,2,1,0};
    static const uint8_t swipe[]={1,2,3,3,2,1};
    static const uint8_t tail[]={0,1,2,3,3,2,1,0};
    static const uint8_t combi[]={0,0,0,1,2,3,3,3,4,1};
    static const uint8_t blades[]={0,1,2,3,4,5};
    static const uint8_t cannon[]={0,0,1,1,1,2,2,2,1,1};
    static const uint8_t disc[]={0,1,2,3,4,5}; /* AMP.S disc_prepare. */
    static const uint8_t marine_pulse[]={0,1,2,2,3,2,1,0}; /* AMP.S crouch_frames. */
    static const uint8_t marine_standing[]={0,1,2,1}; /* AMP.S launchfire / recoil. */
    const uint8_t *frames=0;unsigned count=0,seq=AS_STAND;
#define POSE_SEQ(v,s) do{frames=(v);count=sizeof(v)/sizeof((v)[0]);seq=(s);}while(0)
    if(species==0u){
        /* MAZESCRN.S human_defs: 1 shotgun, 2 pulse, 3 flame, 4 smartgun.
         * Enemy AMP.S: AS_FIGHT1=crouched pulse, AS_FIGHT2=standing flame. */
        if(weapon==1u || weapon==3u){POSE_SEQ(marine_standing,AS_FIGHT2);}
        else if(weapon==2u || weapon==4u){POSE_SEQ(marine_pulse,AS_FIGHT1);}
    }
    else if(species==1u){
        if(weapon==1u){POSE_SEQ(tail,AS_FIGHT3);}
        else if(weapon==2u){POSE_SEQ(swipe,AS_FIGHT2);}
        else if(weapon==3u){POSE_SEQ(bite,AS_FIGHT1);}
    }else if(species==2u){
        if(weapon==1u){POSE_SEQ(combi,AS_FIGHT5);}
        else if(weapon==2u){POSE_SEQ(cannon,AS_FIGHT3);}
        else if(weapon==3u){POSE_SEQ(disc,AS_FIGHT4);}
        else if(weapon==4u){POSE_SEQ(blades,AS_FIGHT6);}
    }
#undef POSE_SEQ
    if(!frames||age>=count)return (AvpMpSpritePose){AS_STAND,0,0};
    return (AvpMpSpritePose){(uint16_t)seq,frames[age],1};
}

/* Frontal Marine attack frames use original AAF indices, not additional
 * gameplay attacks. Shotgun shows three standing-attack steps once per blast;
 * sustained weapons loop their first three firing poses while held.
 * Keep the pre-existing enemy-derived playlists for non-frontal views. */
static inline unsigned avp_mp_sprite_marine_sustained_weapon(unsigned weapon)
{
    return weapon>=2u && weapon<=4u;
}
static inline unsigned avp_mp_sprite_marine_fire_interrupted(int health,
    unsigned dead,unsigned pain)
{
    return health<=0 || dead || pain;
}
/* Only the original MAZESCRN weapon script decides when equipment is
 * ready. Holding Fire during move-out or move-in is not a firing pose.
 * This gate is presentation-only; it never consumes or delays Fire input. */
static inline unsigned avp_mp_sprite_marine_weapon_ready(unsigned weapon,
    int pending_weapon,unsigned deselecting,unsigned equip_phase)
{
    return weapon>=1u && weapon<=4u && pending_weapon==0 &&
        !deselecting && !equip_phase;
}
static inline AvpMpSpritePose avp_mp_sprite_attack_pose_view(unsigned species,
    unsigned weapon,unsigned age,unsigned front,unsigned held)
{
    if(species==0u && front){
        /* The return frame belongs to the next idle pose, not the shot. */
        static const uint8_t shotgun[]={0,1,2};
        static const uint8_t pulse[]={0,1,2};
        static const uint8_t flame[]={0,1,2};
        const uint8_t *frames=0;
        unsigned length=0,seq=AS_STAND;
        if(weapon==1u){frames=shotgun;length=sizeof(shotgun);seq=AS_FIGHT2;}
        else if(weapon==2u || weapon==4u){frames=pulse;length=sizeof(pulse);seq=AS_FIGHT1;}
        else if(weapon==3u){frames=flame;length=sizeof(flame);seq=AS_FIGHT2;}
        if(!frames || (!held && age>=length) ||
           (weapon==1u && age>=length))return (AvpMpSpritePose){AS_STAND,0,0};
        return (AvpMpSpritePose){(uint16_t)seq,frames[age%length],1};
    }
    /* Only the frontal animation is changed. The existing non-front
     * Marine source playlist continues looping while a sustained attack
     * is active, instead of disappearing after the first eight ticks. */
    if(species==0u && held && avp_mp_sprite_marine_sustained_weapon(weapon))
        age%=weapon==3u?4u:8u;
    return avp_mp_sprite_attack_pose(species,weapon,age);
}

/* Marine frontal Pulse Rifle / Smart Gun: the original AAF 18 sheet is
 * standing=0, lowering=1, crouched firing=2. Its frame 3 remains excluded.
 * On entry show 0 -> 1, HOLD the settled crouch for one full native tick,
 * only then cycle crouched firing 2 <-> 1. Show 1 -> 0 once on release.
 * This is strictly cosmetic: native fire, damage and ammo are not delayed.
 * Side/rear observers retain the source playlist. */
enum {
    AVP_MP_MARINE_CROUCH_IDLE=0,
    AVP_MP_MARINE_CROUCH_LOWER=1,
    AVP_MP_MARINE_CROUCH_FIRE=2,
    AVP_MP_MARINE_CROUCH_RAISE=3
};
typedef struct AvpMpMarineCrouchAnim {
    uint8_t phase;
    uint64_t start_tick;
} AvpMpMarineCrouchAnim;

static inline unsigned avp_mp_sprite_marine_crouch_weapon(unsigned weapon)
{
    return weapon==2u || weapon==4u;
}
static inline void avp_mp_sprite_marine_crouch_update(AvpMpMarineCrouchAnim *anim,
    unsigned weapon,unsigned fire_held,unsigned interrupted,uint64_t tick)
{
    unsigned age;
    if(!anim)return;
    if(interrupted || !avp_mp_sprite_marine_crouch_weapon(weapon)){
        anim->phase=AVP_MP_MARINE_CROUCH_IDLE;
        return;
    }
    age=(unsigned)(tick-anim->start_tick);
    if(fire_held){
        if(anim->phase==AVP_MP_MARINE_CROUCH_IDLE ||
           anim->phase==AVP_MP_MARINE_CROUCH_RAISE){
            anim->phase=AVP_MP_MARINE_CROUCH_LOWER;
            anim->start_tick=tick;
        }else if(anim->phase==AVP_MP_MARINE_CROUCH_LOWER && age>=3u){
            anim->phase=AVP_MP_MARINE_CROUCH_FIRE;
            anim->start_tick=tick;
        }
    }else if(anim->phase==AVP_MP_MARINE_CROUCH_LOWER){
        /* A one-tick tap while still upright must not fabricate a crouch. */
        if(age==0u)anim->phase=AVP_MP_MARINE_CROUCH_IDLE;
        else {
            anim->phase=AVP_MP_MARINE_CROUCH_RAISE;
            anim->start_tick=tick;
        }
    }else if(anim->phase==AVP_MP_MARINE_CROUCH_FIRE){
        anim->phase=AVP_MP_MARINE_CROUCH_RAISE;
        anim->start_tick=tick;
    }else if(anim->phase==AVP_MP_MARINE_CROUCH_RAISE && age>=2u){
        anim->phase=AVP_MP_MARINE_CROUCH_IDLE;
    }
}
/* Gameplay stays 10 Hz. The renderer samples the native-tick fraction,
 * allowing visible 20 Hz Pulse Rifle and 40 Hz Smart Gun firing cycles without
 * advancing actual attacks or rushing the one-time lowering/raising poses.
 * The original two-frame crouched fire cycle [2,1] is preserved. */
static inline AvpMpSpritePose avp_mp_sprite_marine_crouch_pose_present(
    const AvpMpMarineCrouchAnim *anim,uint64_t tick,double alpha,
    unsigned weapon)
{
    static const uint8_t lower[]={0,1,1};
    static const uint8_t firing[]={2,1};
    static const uint8_t raise[]={1,0};
    const uint8_t *frames=NULL;
    unsigned age,index;
    if(!anim)return (AvpMpSpritePose){AS_STAND,0,0};
    age=(unsigned)(tick-anim->start_tick);
    if(anim->phase==AVP_MP_MARINE_CROUCH_LOWER && age<3u){
        frames=lower;index=age;
    }else if(anim->phase==AVP_MP_MARINE_CROUCH_FIRE){
        const unsigned speed=weapon==4u?4u:weapon==2u?2u:1u;
        /* Clamp the fractional position, including a defensive alpha==1.
         * No transition tick or native shot is ever generated by sampling. */
        if(alpha<0.0)alpha=0.0;
        if(alpha>=1.0)alpha=0.999999;
        frames=firing;
        index=((age*speed)+(unsigned)(alpha*(double)speed))%2u;
    }else if(anim->phase==AVP_MP_MARINE_CROUCH_RAISE && age<2u){
        frames=raise;index=age;
    }
    else return (AvpMpSpritePose){AS_STAND,0,0};
    return (AvpMpSpritePose){AS_FIGHT1,(uint16_t)frames[index],1};
}
static inline AvpMpSpritePose avp_mp_sprite_marine_crouch_pose(
    const AvpMpMarineCrouchAnim *anim,uint64_t tick)
{
    return avp_mp_sprite_marine_crouch_pose_present(anim,tick,0.0,2u);
}

/* Shotgun: observe the actual native MAZESCRN.S::process_shot ammo
 * transaction, AFTER UpdtScrOverlays, rather than start_action's earlier
 * active_wep toggle. That toggle can occur before the real FIRE opcode and
 * could display an attack early or while switching. The original Banana
 * Stops refill at the final round can INCREASE ammo; inequality covers it. */
static inline unsigned avp_mp_sprite_marine_shot_event(unsigned weapon,
    unsigned fire_held,unsigned weapon_ready,unsigned same_shotgun,
    unsigned ammo_before,unsigned ammo_after)
{
    return weapon==1u && fire_held && weapon_ready && same_shotgun &&
        ammo_before!=ammo_after;
}

/* The enemy Alien's AMP.S::amp_handalien adds five native ticks of
 * non-attack recovery between playlists. Only held-fire VISUAL repeats use
 * the delay; a new input press immediately displays the live attack. */
static inline unsigned avp_mp_sprite_attack_rearm(unsigned species,
    unsigned pressed,unsigned was_pressed,unsigned active,
    uint64_t tick,uint64_t repeat_ready_tick)
{
    if(!pressed)return 0u;
    if(!was_pressed)return 1u;
    if(active)return 0u;
    return species!=1u || tick>=repeat_ready_tick;
}

/* AMP.S go increments AS_RUN frame once per native update for Alien and
 * Predator, but the enemy Marine's chase mode takes alternate updates. A
 * newly moving actor starts at frame 0; idle avatars never keep animating. */
static inline AvpMpSpritePose avp_mp_sprite_motion_pose(unsigned species,
    unsigned moving,unsigned age)
{
    if(!moving)return (AvpMpSpritePose){species==0u?AS_WALK:AS_STAND,0,1};
    return (AvpMpSpritePose){AS_RUN,(uint16_t)(species==0u?age/2u:age),1};
}

/* Original AMP.S invis_frames has a sound marker (-2) at index 9 which
 * consumes no visual tick. AMP.S appear_frames is the six-frame decloak.
 * Only presentation is affected; original PLAYER.S invisflag/state remains
 * authoritative for damage, targeting, sound, and player visibility rules. */
static inline AvpMpSpritePose avp_mp_sprite_predator_cloak_pose(
    unsigned cloaking,unsigned age)
{
    static const uint8_t fade_out[]={0,1,1,1,1,1,1,1,1,1,1,1,2,3,4,5,6};
    static const uint8_t fade_in[]={6,5,4,3,2,0};
    if(cloaking){
        if(age>=sizeof(fade_out))return (AvpMpSpritePose){AS_HIDDEN,0,1};
        return (AvpMpSpritePose){AS_FIGHT1,fade_out[age],1};
    }
    if(age>=sizeof(fade_in))return (AvpMpSpritePose){AS_STAND,0,0};
    return (AvpMpSpritePose){AS_FIGHT1,fade_in[age],1};
}

/* AMP.S amp_recoil -> amp_recover holds AS_KNOCKBACK frame 0 for exactly
 * two native AMP updates for BOTH Alien and Predator enemies. PLAYER.S pain
 * tracks the first-person damage fade for longer and must not be used as a
 * third-person sprite timer. This pose is strictly visual-only. */
static inline AvpMpSpritePose avp_mp_sprite_enemy_recoil_pose(unsigned age)
{
    return age<2u?(AvpMpSpritePose){AS_KNOCKBACK,0,1}:
                  (AvpMpSpritePose){AS_STAND,0,0};
}
static inline AvpMpSpritePose avp_mp_sprite_predator_pain_pose(unsigned age)
{
    return avp_mp_sprite_enemy_recoil_pose(age);
}
/* The first observation must not invent an injury. A fresh decrease in the
 * visible player's health starts/restarts exactly one two-tick recoil. Ignore
 * lethal hits (the death sheet takes priority), and healing. Unsigned tick
 * subtraction deliberately wraps at tick zero to give an inactive age. */
static inline void avp_mp_sprite_enemy_recoil_observe(uint8_t *valid,
    int16_t *last_energy,uint64_t *start_tick,int16_t energy,uint64_t tick)
{
    if(!*valid){
        *valid=1;
        *start_tick=tick-2u;
    }else if(energy>0 && energy<*last_energy){
        *start_tick=tick;
    }
    *last_energy=energy;
}

/* A fatal PvP hit can land after this actor's 10 Hz tick. The corpse is
 * registered on the next actor tick; until then keep the victim visible in
 * frame zero of the SAME original Jaguar death sheet, no blank 60 Hz frame. */
static inline AvpMpSpritePose avp_mp_sprite_pending_death_pose(int energy)
{
    if(energy<=0)return (AvpMpSpritePose){AS_DEATH,0,1};
    return (AvpMpSpritePose){AS_STAND,0,0};
}
static inline AvpMpSpritePose avp_mp_sprite_roar_pose(unsigned age)
{
    /* AMP.S::roar_frames triggered by AVP_FX_EDITR. */
    static const uint8_t frames[]={0,1,2,3,2,4,2,1,1,0,0,0};
    if(age>=sizeof(frames))return (AvpMpSpritePose){AS_STAND,0,0};
    return (AvpMpSpritePose){AS_FIGHT2,frames[age],1};
}
static inline AvpMpSpritePose avp_mp_sprite_stand_pose(unsigned species,unsigned weapon)
{
    /* Original Predator AMP combat sprites: combi FIGHT5, cannon FIGHT3.
     * Wrist blades / smart disc share the unchanged ordinary stand sheet. */
    if(species==2u && weapon==1u)return (AvpMpSpritePose){AS_FIGHT5,0,1};
    if(species==2u && weapon==2u)return (AvpMpSpritePose){AS_FIGHT3,0,1};
    return (AvpMpSpritePose){AS_STAND,0,0};
}
#endif
