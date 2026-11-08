#ifndef AVP_COLLECTABLES_H
#define AVP_COLLECTABLES_H
#include "avp_types.h"

enum AvpCollectType {
    COLL_NONE=0,
    COLL_S1=1,COLL_S2,COLL_S3,COLL_S4,COLL_S5,COLL_S6,COLL_S7,COLL_S8,COLL_S9,COLL_S10,
    COLL_SHOTGUN,COLL_SHOTAMMO,COLL_PULSERIFLE,COLL_PULSEAMMO,COLL_GRENADES,
    COLL_FLAMETHROWER,COLL_FLAMEAMMO,COLL_SMARTGUN,COLL_SMARTAMMO,COLL_MTRACKER,
    COLL_FOOD,COLL_MEDAID
};

enum AvpCollectNotice {
    AVP_CNOTICE_NONE=0, AVP_CNOTICE_CARD, AVP_CNOTICE_SHOTGUN, AVP_CNOTICE_SHOT_AMMO,
    AVP_CNOTICE_PULSE, AVP_CNOTICE_PULSE_AMMO, AVP_CNOTICE_FLAME, AVP_CNOTICE_FLAME_AMMO,
    AVP_CNOTICE_SMART, AVP_CNOTICE_SMART_AMMO, AVP_CNOTICE_MTRACKER,
    AVP_CNOTICE_FOOD, AVP_CNOTICE_MEDAID
};

typedef struct AvpAmmoInfo {
    u16 cur,max,old,rescale;
    s16 x,y;
    u16 dec,inc;
} AvpAmmoInfo;

typedef void (*AvpCollectNoticeFn)(enum AvpCollectNotice notice,unsigned detail);
typedef void (*AvpCollectSfxFn)(unsigned cue);

void ResetClct(void);
void Collectables(void);
/* Read-only original item/corpse lookup; does not collect or mutate. */
int avp_collect_peek(unsigned level,unsigned x,unsigned y,s16 def,
                     s16 *type,s16 *amount);
int CollectIt(s16 type,s16 amount);
int avp_give_energy(s16 amount,enum AvpCollectNotice notice,unsigned sfx);
int avp_give_ammo(unsigned weapon,s16 amount,enum AvpCollectNotice notice,unsigned sfx);
void avp_give_weapon(unsigned weapon,s16 initial_ammo,enum AvpCollectNotice ammo_notice,
                     enum AvpCollectNotice weapon_notice,unsigned weapon_sfx,unsigned ammo_sfx);
void avp_collect_set_callbacks(AvpCollectNoticeFn notice,AvpCollectSfxFn sfx);
/* Co-op-only actor scope: when non-NULL, security keycards are shared world
 * objects with independent per-player collection records. NULL retains the
 * exact original single-player consume-on-pickup behavior. This pointer is
 * only bound around an actor's original player tick on the main thread. */
void avp_collect_mp_keycard_owner(u16 *collected_mask);

/* Multiplayer-only collection eligibility/commit hook. A physical world AMP is
 * available once PER player life, not consumed for the entire co-op world.
 * Calling with commit=0 must not mutate ownership. Only call commit=1 after
 * CollectIt() actually grants or accepts the pickup. NULL keeps untouched
 * original single-player behavior. */
typedef int (*AvpCollectMpItemClaimFn)(void *user,unsigned level,
                                    unsigned amp_slot,s16 type,int commit);
void avp_collect_set_mp_item_claim(AvpCollectMpItemClaimFn fn,void *user);
/* Deathmatch ONLY: replace a non-keycard authored corpse's fixed loot with
 * a per-Marine once-only draw. A NULL callback leaves Jaguar original loot
 * unchanged. Return/claim semantics are owned by the host, not the AMP. */
typedef void (*AvpCollectMpBodyLootFn)(void *user,unsigned level,
                                     unsigned amp_slot);
void avp_collect_set_mp_body_loot(AvpCollectMpBodyLootFn fn,void *user);

extern AvpAmmoInfo ammo_info[6];
extern s16 medpak;
extern s16 show_mt;
extern u8 acs_level;

#endif
