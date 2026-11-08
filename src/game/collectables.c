/* Readable C reconstruction of the collection-effect core in MAZE/PLAYER.S:
 * CollectIt, give_energy, give_wep and give_ammo.
 *
 * Includes spatial object-grid lookup/body specialization plus the inventory,
 * security-card, ammo, weapon and energy semantics from the retail source.
 */
#include "collectables.h"
#include "player.h"
#include "weapons.h"
#include "amp.h"
#include "levels.h"
#include "maze.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <stddef.h>

#define PMED_MAX 1000

AvpAmmoInfo ammo_info[6];
s16 medpak;
s16 show_mt;
/* acs_level is owned by doors.c, matching the original access subsystem. */
extern u8 acs_level;

static AvpCollectNoticeFn notice_fn;
static AvpCollectSfxFn sfx_fn;
static u16 *mp_keycard_owner;
void avp_collect_mp_keycard_owner(u16 *collected_mask)
{ mp_keycard_owner=collected_mask; }

static AvpCollectMpItemClaimFn mp_item_claim;
static void *mp_item_user;
static AvpCollectMpBodyLootFn mp_body_loot;
static void *mp_body_user;
void avp_collect_set_mp_body_loot(AvpCollectMpBodyLootFn fn,void *user)
{ mp_body_loot=fn;mp_body_user=fn?user:NULL; }

void avp_collect_set_mp_item_claim(AvpCollectMpItemClaimFn fn,void *user)
{ mp_item_claim=fn; mp_item_user=fn?user:NULL; }

enum {
    CSFX_SECURITY=AVP_FX_SECUR, CSFX_WEAPON_SHOT=AVP_FX_WHATO,
    CSFX_WEAPON_PULSE=AVP_FX_BUGSO, CSFX_WEAPON_FLAME=AVP_FX_ILLTE,
    CSFX_WEAPON_SMART=AVP_FX_ROKNR, CSFX_PICKUP=AVP_FX_DIST1,
    CSFX_MTRACK=AVP_FX_RUNBU, CSFX_PRED_ENERGY=AVP_FX_CHATC
};

void avp_collect_set_callbacks(AvpCollectNoticeFn notice,AvpCollectSfxFn sfx)
{ notice_fn=notice; sfx_fn=sfx; }

typedef struct BodyLoot { s16 x,y,type,amount; } BodyLoot;
#define END_BODY { -1,0,0,0 }
static const BodyLoot l1_bodies[]={ {0x22,0x21,COLL_S7,0},{0x22,0x0e,COLL_PULSERIFLE,60},END_BODY };
static const BodyLoot l2_bodies[]={ {0x1e,0x29,COLL_S3,0},{0x29,0x14,COLL_PULSERIFLE,60},{0x28,0x1b,COLL_FLAMETHROWER,25},END_BODY };
static const BodyLoot l3_bodies[]={ {0x24,0x09,COLL_SHOTGUN,15},{0x22,0x11,COLL_S1,0},{0x08,0x2a,COLL_S2,0},{0x22,0x19,COLL_SHOTAMMO,15},{0x1f,0x1c,COLL_SHOTAMMO,15},{0x1c,0x1e,COLL_SHOTAMMO,15},{0x08,0x08,COLL_SHOTAMMO,15},{0x04,0x23,COLL_FOOD,100},{0x07,0x21,COLL_FOOD,100},{0x21,0x0a,COLL_FOOD,100},{0x24,0x11,COLL_FOOD,100},{0x25,0x21,COLL_MEDAID,300},{0x26,0x28,COLL_MEDAID,300},{0x22,0x28,COLL_MEDAID,300},{0x0e,0x2a,COLL_PULSERIFLE,60},END_BODY };
static const BodyLoot l4_bodies[]={ {0x17,0x11,COLL_S4,0},{0x0c,0x13,COLL_S6,0},{0x2c,0x12,COLL_PULSERIFLE,60},{0x2a,0x18,COLL_FLAMETHROWER,25},{0x1d,0x24,COLL_PULSERIFLE,60},END_BODY };
static const BodyLoot l5_bodies[]={ {0x19,0x27,COLL_S5,0},{0x22,0x0e,COLL_MTRACKER,0},END_BODY };
static const BodyLoot l14_bodies[]={ {0x0b,0x23,COLL_PULSEAMMO,60},{0x09,0x16,COLL_PULSEAMMO,60},{0x10,0x0d,COLL_PULSEAMMO,60},{0x10,0x0a,COLL_MEDAID,300},{0x08,0x0b,COLL_PULSEAMMO,60},{0x2e,0x06,COLL_PULSEAMMO,60},{0x2d,0x26,COLL_PULSEAMMO,60},{0x2d,0x23,COLL_PULSEAMMO,60},{0x2d,0x1b,COLL_PULSEAMMO,60},{0x26,0x09,COLL_PULSEAMMO,60},{0x14,0x0d,COLL_PULSEAMMO,60},{0x17,0x0a,COLL_PULSEAMMO,60},{0x24,0x1b,COLL_PULSEAMMO,60},{0x08,0x1f,COLL_SHOTAMMO,15},{0x0b,0x16,COLL_SHOTAMMO,15},{0x0f,0x06,COLL_SHOTAMMO,15},{0x32,0x08,COLL_SHOTAMMO,15},{0x2f,0x08,COLL_SHOTAMMO,15},{0x26,0x23,COLL_SHOTAMMO,15},{0x14,0x0a,COLL_SHOTAMMO,15},{0x0b,0x03,COLL_GRENADES,8},{0x0a,0x03,COLL_S8,0},{0x21,0x0b,COLL_S10,0},END_BODY };
static const BodyLoot l15_bodies[]={ {0x0b,0x15,COLL_S9,0},{0x17,0x0c,COLL_SHOTAMMO,15},{0x25,0x0a,COLL_SHOTAMMO,15},{0x25,0x17,COLL_SHOTAMMO,15},{0x02,0x08,COLL_MEDAID,300},{0x24,0x17,COLL_MEDAID,300},{0x28,0x12,COLL_MEDAID,300},{0x19,0x0a,COLL_GRENADES,8},{0x2a,0x10,COLL_SMARTGUN,0},END_BODY };
static const BodyLoot no_bodies[]={END_BODY};
static const BodyLoot *cur_bodies=no_bodies;

static const BodyLoot *body_loot_for_level(unsigned level)
{
    static const BodyLoot *const table[AVP_MAX_LEVEL]={l1_bodies,l2_bodies,l3_bodies,l4_bodies,l5_bodies,no_bodies,no_bodies,no_bodies,no_bodies,no_bodies,no_bodies,no_bodies,no_bodies,l14_bodies,l15_bodies};
    return level>=1u && level<=AVP_MAX_LEVEL ? table[level-1u] : no_bodies;
}
void ResetClct(void)
{
    cur_bodies=body_loot_for_level((unsigned)cur_level);
}

static int collect_info_for(s16 def,s16 *type,s16 *amount)
{
    *type=0;*amount=0;
    switch(def){
    case 11:*type=COLL_MEDAID;*amount=300;break;
    case 12:*type=COLL_SMARTAMMO;*amount=20;break;
    case 15:*type=COLL_FLAMEAMMO;*amount=25;break;
    case 16:*type=COLL_PULSEAMMO;*amount=60;break;
    case 18:*type=-1;break; /* COLL_BODY */
    case 21:*type=COLL_FOOD;*amount=100;break;
    case 23:*type=COLL_SMARTAMMO;*amount=20;break;
    case 24:*type=COLL_PULSEAMMO;*amount=60;break;
    case 25:*type=COLL_SHOTAMMO;*amount=15;break;
    case 38:case 39:case 40:*type=-1;break; /* dead bodies */
    default:break;
    }
    return *type!=0;
}

/* Read-only exposure of the SAME original pickup definitions used by
   Collectables(). Bots may inspect source loot without consuming it or
   changing per-player inventory, original AMP ownership, or world state. */
int avp_collect_peek(unsigned level,unsigned x,unsigned y,s16 def,
                     s16 *type,s16 *amount)
{
    const BodyLoot *b;
    if(!type||!amount||!collect_info_for(def,type,amount))return 0;
    if(*type!=-1)return 1;
    for(b=body_loot_for_level(level);b->x>=0;++b)
        if((unsigned)b->x==x && (unsigned)b->y==y){
            *type=b->type;*amount=b->amount;return 1;
        }
    if(def==39){*type=COLL_SHOTAMMO;*amount=15;return 1;}
    if(def==38){*type=COLL_PULSEAMMO;*amount=60;return 1;}
    if(def==40){*type=COLL_MEDAID;*amount=300;return 1;}
    return 0; /* Unsupported corpse: never fabricate a reward. */
}

/* MAZE/PLAYER.S uses the authored per-level body table to resolve loot
 * against a corpse AMP.  The host must also recover such a corpse when its
 * grid entry has not been written yet (or was overwritten by another AMP),
 * without creating a separate pickup or losing the original collected bit.
 * This applies to weapons/ammo/medkits as well as access cards. */
static u16 authored_body_at(unsigned x,unsigned y)
{
    const BodyLoot *b;
    unsigned i;
    for(b=cur_bodies;b->x>=0;++b){
        if((unsigned)b->x!=x||(unsigned)b->y!=y)continue;
        for(i=0;i<AVP_NUM_AMPS;++i){
            const AvpAmp *a=&amp_data[i];
            if(a->astype!=18&&a->astype!=38&&a->astype!=39&&a->astype!=40)continue;
            if(((u32)a->xpos>>16)==x&&((u32)a->ypos>>16)==y&&
               !(a->flags&(1u<<AMP_COLLECT)))return (u16)(i+1u);
        }
    }
    return 0;
}

/* Co-op items have per-player claims, whereas a legacy single-player body
 * can already carry AMP_COLLECT in a retained/rebuilt world. Do not discard
 * a REAL, source-authored security-card corpse for an unclaimed co-op Marine
 * merely because its collected flag survived or objmap now owns another item.
 * Match the real corpse AMP at the exact authored cell; never synthesize a
 * card from the nearby food corpse or from a recycled/absent AMP slot.
 * Return 0 unless the actual player is still eligible for this exact item. */
static u16 coop_unclaimed_card_body(unsigned x,unsigned y)
{
    const BodyLoot *b;
    unsigned i;
    if(!mp_item_claim || player_type!=PT_HUMAN)return 0;
    for(b=cur_bodies;b->x>=0;++b){
        if((unsigned)b->x!=x||(unsigned)b->y!=y ||
           b->type<COLL_S1 || b->type>COLL_S10)continue;
        for(i=0;i<AVP_NUM_AMPS;++i){
            const AvpAmp *a=&amp_data[i];
            /* A corpse is an authored collectable: 38/39/40. Type 18 is
             * the live cocoon and must NOT stand in for a card body. */
            if(a->astype!=38 && a->astype!=39 && a->astype!=40)continue;
            if(((u32)a->xpos>>16)!=x || ((u32)a->ypos>>16)!=y)
                continue;
            /* Co-op card ownership belongs to the REAL authored corpse,
             * not to its incidental mode/host_static bits. Level resets,
             * world snapshots and entrance cleanup may leave both bits
             * cleared before the player first reaches the corpse. Requiring
             * either one made Food in objmap win until the next respawn.
             * Type AND exact authored cell are still mandatory: an empty,
             * moved or reused AMP slot can never create a phantom card. */
            if(mp_item_claim(mp_item_user,(unsigned)cur_level,i,b->type,0))
                return (u16)(i+1u);
        }
    }
    return 0;
}

void Collectables(void)
{
    unsigned x=(unsigned)(x_pos>>16),y=(unsigned)(y_pos>>16),off;
    u16 no,grid_no,coop_card_no=0;AvpAmp *a;s16 type,amount;int body=0;
    /* Multiplayer binds original worlds serially, changing cur_level without
     * necessarily re-running InitPlayer/ResetClct. The authored corpse-loot
     * table must belong to this world, not the last world that was entered.
     * ResetClct only selects a static table; it does not reset picked items. */
    ResetClct();
    if(x>=AVP_AMP_GRID_W||y>=AVP_AMP_GRID_H)return;
    off=y*AVP_AMP_OBJ_ROW_BYTES+x*2u;
    no=(u16)(((u16)objmap[off]<<8)|objmap[off+1]);
    grid_no=no;
    /* Choose an unclaimed authored card before the objmap's last writer.
     * Keep the selected identity for the AMP_COLLECT legacy-world override. */
    coop_card_no=coop_unclaimed_card_body(x,y);
    if(coop_card_no)no=coop_card_no;
    if(!no||no>AVP_NUM_AMPS ||
       /* objmap can outlive a moved/reused AMP slot.  A stale owner
        * must never collect a pickup from a different square.  Keep the
        * authored corpse fallback for legitimate missing grid entries. */
       ((u32)amp_data[no-1u].xpos>>16)!=x ||
       ((u32)amp_data[no-1u].ypos>>16)!=y ||
       !collect_info_for(amp_data[no-1u].astype,&type,&amount) ||
       (type==-1 && (amp_data[no-1u].flags&(1u<<AMP_COLLECT)) &&
        no!=coop_card_no) ||
       /* Collecting a normal pickup marks its AMP inactive.  If the old
        * objmap number is restored later, its previous item must not be
        * awarded a second time from a dead/recycled slot.  Corpse fallback
        * is handled independently below. */
       (type!=-1 && !amp_data[no-1u].mode && !amp_data[no-1u].host_static)){
        /* Preserve the live object-grid's normal priority.  Only fall back
           to an actual, unlooted authored body if its entry is unavailable. */
        no=authored_body_at(x,y);
        if(!no){
            /* The obsolete owner occupies no live item at this square.
             * Clearing this entry prevents it from shadowing later items. */
            objmap[off]=objmap[off+1u]=0;
            return;
        }
    }
    a=&amp_data[no-1u];
    if(!collect_info_for(a->astype,&type,&amount))return;
    if(type==-1){
        const BodyLoot *b;
        body=1;
        if((a->flags&(1u<<AMP_COLLECT)) && no!=coop_card_no){
            if(grid_no==no)objmap[off]=objmap[off+1]=0;
            return;
        }
        for(b=cur_bodies;b->x>=0;++b)if((unsigned)b->x==x&&(unsigned)b->y==y){type=b->type;amount=b->amount;break;}
        if(b->x<0){
            if(a->astype==39){type=COLL_SHOTAMMO;amount=15;}
            else if(a->astype==38){type=COLL_PULSEAMMO;amount=60;}
            else if(a->astype==40){type=COLL_MEDAID;amount=300;}
            else if(mp_body_loot){type=COLL_NONE;amount=0;}
            else {if(grid_no==no)objmap[off]=objmap[off+1]=0;return;}
        }
    }
    /* Deathmatch's optional random corpse loot NEVER replaces authored
     * security cards, which are needed to traverse locked retail maps.
     * With callback NULL, the original CollectIt() branch is untouched. */
    if(body && mp_body_loot && !(type>=COLL_S1 && type<=COLL_S10)){
        mp_body_loot(mp_body_user,(unsigned)cur_level,(unsigned)(no-1u));
        return;
    }
    /* Local co-op shares the physical object, but its benefit is claimed
     * separately by each living player. This is also true for weapons,
     * ammunition, food, medkits and the motion tracker. A full ammo/health
     * pool must NOT consume this player's claim: CollectIt() reports 0.
     * Original AMP mode, collect flag and object-grid are not modified.
     * The single-player path below is byte-for-byte unchanged. */
    if(mp_item_claim){
        if(!mp_item_claim(mp_item_user,(unsigned)cur_level,
                          (unsigned)(no-1u),type,0))return;
        if(!CollectIt(type,amount))return;
        (void)mp_item_claim(mp_item_user,(unsigned)cur_level,
                            (unsigned)(no-1u),type,1);
        return;
    }
    /* In co-op an authored Security Keycard must remain in its original
     * AMP/corpse and objmap for EVERY other participant. Track a card once
     * per player/life; don't repeatedly award it while standing on the body.
     * The original single-player removal path below is unchanged. */
    if(mp_keycard_owner && type>=COLL_S1 && type<=COLL_S10){
        const u16 bit=(u16)(1u<<((unsigned)type-COLL_S1));
        if(!(*mp_keycard_owner & bit) && CollectIt(type,amount))
            *mp_keycard_owner=(u16)(*mp_keycard_owner | bit);
        return;
    }
    if(!CollectIt(type,amount))return;
    if(body)a->flags|=(1u<<AMP_COLLECT);
    else {
        /* A collected pickup is a free AMP, not an unchanging display prop.
         * Do not save its old host_static state on the next level exit. */
        a->mode=NULL;
        a->host_static=0;
    }
    /* When the original slot is held by another AMP, an authored corpse
       fallback must not erase that unrelated grid entry. */
    if(grid_no==no)objmap[off]=objmap[off+1]=0;
    /* PLAYER.S .killamp only does CLR.L amp_mode: the collected pickup's
     * slot is NOT returned to l1_amp_free (AMP.S calls amp_release only for
     * specific events).  Releasing it here let the next amp_req reuse the
     * slot, changing later AMP numbering and UpdateAMPs order vs retail. */
}

static void notify(enum AvpCollectNotice n,unsigned detail)
{ if (notice_fn && n!=AVP_CNOTICE_NONE) notice_fn(n,detail); }
static void sfx(unsigned n)
{
    if (!n) return;
    if (sfx_fn) sfx_fn(n);
    else {
        const AvpRuntimeOps *o=avp_runtime_ops();
        if (o->play_sfx) o->play_sfx(o->user,n);
    }
}

int avp_give_energy(s16 amount,enum AvpCollectNotice notice,unsigned sound)
{
    s16 *dst=&player_energy;
    s16 cap=max_energy;
    s32 v;
    if (player_type==PT_PREDATOR) { dst=&medpak; cap=PMED_MAX; }
    if (*dst==cap) return 0;
    v=(s32)*dst+(s32)amount;
    if (v>cap) v=cap;
    *dst=(s16)v;
    /* On Jaguar the Predator's medpak charge is also weapon slot 6's ammo
     * word, used by the HUD and EEPROM save.  The C reconstruction keeps
     * the two names in separate objects, so mirror the retail alias. */
    if (player_type==PT_PREDATOR) ammo_info[5].cur=(u16)medpak;
    notify(notice,0);
    sfx(sound);
    return 1;
}

int avp_give_ammo(unsigned weapon,s16 amount,enum AvpCollectNotice notice,unsigned sound)
{
    AvpAmmoInfo *a;
    s32 v;
    if (weapon<1 || weapon>6) return 0;
    a=&ammo_info[weapon-1];
    if (a->cur==a->max) return 0;
    v=(s32)a->cur+(s32)amount;
    if (v>a->max) v=a->max;
    a->cur=(u16)v;
    if(player_type==PT_PREDATOR&&weapon==6u)medpak=(s16)a->cur;
    notify(notice,weapon);
    sfx(sound);
    return 1;
}

void avp_give_weapon(unsigned weapon,s16 initial_ammo,enum AvpCollectNotice ammo_notice,
                     enum AvpCollectNotice weapon_notice,unsigned weapon_sfx,unsigned ammo_sfx)
{
    int newly_given;
    if (weapon>7) return;
    newly_given=!(cur_weps&(1u<<weapon));
    if (newly_given) {
        cur_weps=(u8)(cur_weps|(1u<<weapon));
        if ((s16)weapon>cur_wepno) new_wepno=(s16)weapon;
        notify(weapon_notice,weapon);
        if (weapon_sfx) { sfx(weapon_sfx); ammo_sfx=0; }
    }
    /* PLAYER.S give_wep sets a0 to the weapon notice before falling through
     * into give_ammo.  The fall-through writes that same notice again.  A
     * separately collected ammo item instead writes its own new_message. */
    (void)avp_give_ammo(weapon,initial_ammo,
                       newly_given?weapon_notice:ammo_notice,ammo_sfx);
}

int CollectIt(s16 type,s16 amount)
{
    unsigned pick=CSFX_PICKUP;

    /* Food and med-aid are the only collection classes reachable by Predator;
     * all cards/weapons/ammo are skipped by the .nrg_only branch. */
    if (player_type!=PT_PREDATOR) {
        if (type>=COLL_S1 && type<=COLL_S10) {
            if ((u8)type>acs_level) {
                acs_level=(u8)type;
                notify(AVP_CNOTICE_CARD,(unsigned)type);
                sfx(CSFX_SECURITY);
            }
            /* A card is removed even if the player already has this/higher
             * access: original .kill path always succeeds. */
            return 1;
        }
        switch(type) {
        case COLL_SHOTGUN:
            avp_give_weapon(1,amount,AVP_CNOTICE_SHOT_AMMO,AVP_CNOTICE_SHOTGUN,
                            CSFX_WEAPON_SHOT,CSFX_PICKUP); return 1;
        case COLL_SHOTAMMO:
            return avp_give_ammo(1,amount,AVP_CNOTICE_SHOT_AMMO,CSFX_PICKUP);
        case COLL_PULSERIFLE:
            avp_give_weapon(2,amount,AVP_CNOTICE_PULSE_AMMO,AVP_CNOTICE_PULSE,
                            CSFX_WEAPON_PULSE,CSFX_PICKUP); return 1;
        case COLL_PULSEAMMO:
            return avp_give_ammo(2,amount,AVP_CNOTICE_PULSE_AMMO,CSFX_PICKUP);
        case COLL_FLAMETHROWER:
            avp_give_weapon(3,amount,AVP_CNOTICE_FLAME_AMMO,AVP_CNOTICE_FLAME,
                            CSFX_WEAPON_FLAME,CSFX_PICKUP); return 1;
        case COLL_FLAMEAMMO:
            return avp_give_ammo(3,amount,AVP_CNOTICE_FLAME_AMMO,CSFX_PICKUP);
        case COLL_SMARTGUN:
            avp_give_weapon(4,amount,AVP_CNOTICE_SMART_AMMO,AVP_CNOTICE_SMART,
                            CSFX_WEAPON_SMART,CSFX_PICKUP); return 1;
        case COLL_SMARTAMMO:
            return avp_give_ammo(4,amount,AVP_CNOTICE_SMART_AMMO,CSFX_PICKUP);
        case COLL_MTRACKER:
            show_mt=-1; notify(AVP_CNOTICE_MTRACKER,0); sfx(CSFX_MTRACK); return 1;
        default: break;
        }
    }

    if (type==COLL_FOOD)
        return avp_give_energy(amount,AVP_CNOTICE_FOOD,
                               player_type==PT_PREDATOR?CSFX_PRED_ENERGY:pick);
    if (type==COLL_MEDAID)
        return avp_give_energy(amount,AVP_CNOTICE_MEDAID,
                               player_type==PT_PREDATOR?CSFX_PRED_ENERGY:pick);
    return 0;
}
