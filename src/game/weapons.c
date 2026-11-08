/* Readable C reconstruction of the Predator honour-system weapon availability
 * routines in MAZE/MAZESCRN.S (InitPredAvail / PredAvail).
 *
 * Weapon numbers deliberately retain the original bit positions:
 *   1 combi-stick, 2 shoulder cannon, 3 smart disc, 4 base weapon.
 */
#include "weapons.h"
#include "player.h"
#include <stddef.h>

u8 cur_weps,old_weps;
s16 cur_wepno,new_wepno;

struct PredAward {
    s32 score;
    u8 weapon;
    enum AvpWeaponNotice gain,lose;
};

static const struct PredAward awards[]={
    {0,4,AVP_WNOTICE_NONE,AVP_WNOTICE_NONE},
    {150000,1,AVP_WNOTICE_GAIN_COMBI,AVP_WNOTICE_LOSE_COMBI},
    {350000,3,AVP_WNOTICE_GAIN_DISC,AVP_WNOTICE_LOSE_DISC},
    {750000,2,AVP_WNOTICE_GAIN_CANNON,AVP_WNOTICE_LOSE_CANNON}
};
static unsigned pred_index;
static int mp_predator_dm;
void avp_weapons_mp_predator_deathmatch(int enabled)
{mp_predator_dm=enabled!=0;}
static AvpWeaponNoticeFn notice_fn;
static AvpWeaponSfxFn sfx_fn;

enum { SFX_PRED_LOSE=1, SFX_PRED_GAIN=2 };

void avp_weapons_set_callbacks(AvpWeaponNoticeFn notice,AvpWeaponSfxFn sfx)
{ notice_fn=notice; sfx_fn=sfx; }

void avp_weapons_cycle(int direction,int maxslot)
{
    int base=new_wepno>=1&&new_wepno<=maxslot?new_wepno:cur_wepno;
    /* Slots 5/6 are Predator cloak and medkit actions, not switchable guns.
     * Slot 0 is the Marine's special weaponless starting state. */
    if(maxslot>4)maxslot=4;
    if(!direction||maxslot<1)return;
    if(base<1||base>maxslot)base=1;
    for(int n=0;n<maxslot;n++){
        base+=direction>0?1:-1;
        if(base>maxslot)base=1;else if(base<1)base=maxslot;
        if(cur_weps&(1u<<base)){new_wepno=(s16)base;return;}
    }
}

void InitPredAvail(void) { pred_index=0; }

void PredAvail(void)
{
    /* Deathmatch combat weapons are intentionally independent of honour.
     * Do not auto-select or revoke a gun during a native weapon tick. */
    if(mp_predator_dm){
        cur_weps=(u8)(cur_weps|0x1eu);
        return;
    }
    /* MAZESCRN.S:1437-1486.  Thresholds compare UNSIGNED (BHS/BLO) against
     * the 32-bit score.  On the way down only cur_wepno is compared with the
     * revoked weapon; new_wepno may legitimately name a weapon revoked by a
     * later step of the same loop -- there is no fallback in the original. */
    u32 sc=(u32)score;
    int changed=0;

    while (pred_index>0 && sc<(u32)awards[pred_index].score) {
        const struct PredAward *a=&awards[pred_index];
        cur_weps=(u8)(cur_weps & (u8)~(1u<<a->weapon));
        if (notice_fn) notice_fn(a->lose);
        if (cur_wepno==(s16)a->weapon)
            new_wepno=(s16)awards[pred_index-1].weapon;
        --pred_index;
        changed=-1;
    }
    if (changed) {
        if (sfx_fn) sfx_fn(SFX_PRED_LOSE);
        return;
    }

    /* Rising can cross more than one threshold in a single update. */
    while (pred_index+1<sizeof(awards)/sizeof(awards[0]) &&
           sc>=(u32)awards[pred_index+1].score) {
        const struct PredAward *a=&awards[++pred_index];
        cur_weps=(u8)(cur_weps | (u8)(1u<<a->weapon));
        new_wepno=(s16)a->weapon;
        if (notice_fn) notice_fn(a->gain);
        changed=1;
    }
    if (changed && sfx_fn) sfx_fn(SFX_PRED_GAIN);
}

#ifdef AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE
/* Optional native private-state seam, with no change to PredAvail(). */
unsigned avp_mp_weapons_native_get_index(void) { return pred_index; }
int avp_mp_weapons_native_set_index(unsigned index)
{
    if (index>=sizeof(awards)/sizeof(awards[0])) return 0;
    pred_index=index;
    return 1;
}
#endif
