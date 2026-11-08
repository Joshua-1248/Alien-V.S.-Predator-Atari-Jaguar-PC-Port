/* Included only at tail of original MAZESCRN.S C translation. All private
 * original variables are lexically visible here. No original code changed.
 */
#include "../../new/multiplayer/local_mp_weapon_native.h"
#include <string.h>
#define AVP_MP_WEAPON_MAGIC 0x5745504Eu

static int avp_mp_known_weapon_def(const AvpWeaponDef *p)
{
    static const s16 types[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
    if (!p) return 1;
    for (unsigned t=0;t<3u;t++)
        for (s16 n=1;n<=6;n++)
            if (avp_weapon_def(types[t],n)==p) return 1;
    return 0;
}

/* Pointers in a live script may point midway through a command stream (or
 * immediately past END_ANIM after completing the last frame).  Recognize
 * only addresses derived from the game's own immutable authored scripts;
 * do not dereference any caller-provided address. */
static int avp_mp_in_original_stream(const AvpWepCmd *p,
                                      const AvpWepCmd *stream)
{
    if (!stream) return 0;
    for (unsigned i=0;i<256u;i++) {
        if (p==stream+i) return 1;
        if (stream[i].op==AVP_WC_END_ANIM)
            return p==stream+i+1u;
    }
    return 0;
}
static int avp_mp_known_weapon_stream(const AvpWepCmd *p)
{
    static const s16 types[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
    if (!p) return 1;
    for (unsigned t=0;t<3u;t++)
        for (s16 n=1;n<=6;n++) {
            const AvpWeaponDef *d=avp_weapon_def(types[t],n);
            if (!d) continue;
            if (avp_mp_in_original_stream(p,d->frame0) ||
                avp_mp_in_original_stream(p,d->move_in) ||
                avp_mp_in_original_stream(p,d->move_out)) return 1;
            for (unsigned j=0;j<2u;j++)
                if (avp_mp_in_original_stream(p,d->actions[j])) return 1;
        }
    return 0;
}

int avp_mp_weapon_native_valid(const AvpMpWeaponEngineState *s)
{
    return s && s->valid==AVP_MP_WEAPON_MAGIC &&
        s->num_weps>=0 && s->num_weps<=6 &&
        s->pred_unlock_index<4u &&
        avp_mp_known_weapon_def(s->cur_def) &&
        avp_mp_known_weapon_stream(s->wep_animptr) &&
        avp_mp_known_weapon_stream(s->wait_ptr);
}

int avp_mp_weapon_native_capture(AvpMpWeaponEngineState *out)
{
    AvpMpWeaponEngineState state;
    if (!out) return 0;
    memset(&state,0,sizeof(state));
    state.valid=AVP_MP_WEAPON_MAGIC;
#define AVP_MP_CAPTURE_WEP(type,name) state.name=name;
    AVP_MP_WEAPON_FIELDS(AVP_MP_CAPTURE_WEP)
#undef AVP_MP_CAPTURE_WEP
    memcpy(state.ammo_info,ammo_info,sizeof(state.ammo_info));
    state.pred_unlock_index=avp_mp_weapons_native_get_index();
    if (!avp_mp_weapon_native_valid(&state)) return 0;
    *out=state;
    return 1;
}

int avp_mp_weapon_native_restore(const AvpMpWeaponEngineState *s)
{
    if (!avp_mp_weapon_native_valid(s)) return 0;
    if (!avp_mp_weapons_native_set_index(s->pred_unlock_index)) return 0;
    /* Validate BEFORE any mutation.  This does not call gameplay commands. */
#define AVP_MP_RESTORE_WEP(type,name) name=s->name;
    AVP_MP_WEAPON_FIELDS(AVP_MP_RESTORE_WEP)
#undef AVP_MP_RESTORE_WEP
    memcpy(ammo_info,s->ammo_info,sizeof(s->ammo_info));
    return 1;
}
