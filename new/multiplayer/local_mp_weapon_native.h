#ifndef AVP_LOCAL_MP_WEAPON_NATIVE_H
#define AVP_LOCAL_MP_WEAPON_NATIVE_H

#include "mazescrn.h"
#include "collectables.h"
#include "weapons.h"
#include <stdint.h>

/* First-person MAZESCRN.S weapon-script state only. These mutable values
 * belong to a player, unlike original host display buffers, runtime output
 * callbacks, or shared audio voices. See the audit before using for gameplay.
 */
#define AVP_MP_WEAPON_FIELDS(X) \
    X(s16, fire_damage) \
    X(u32, fire_distance) \
    X(u16, fire_width) \
    X(s16, num_weps) \
    X(s16, active_wep) \
    X(s16, wep_action) \
    X(s32, wep_x) \
    X(s32, wep_y) \
    X(s32, wep_xvel) \
    X(s32, wep_yvel) \
    X(s16, swing_x) \
    X(s16, swing_y) \
    X(s16, swing_max) \
    X(u8, swing_on) \
    X(u8, wait_state) \
    X(u8, wep_desel) \
    X(s16, hugkill) \
    X(s16, hug_recov) \
    X(s16, hug_throw) \
    X(s16, unstick_dir) \
    X(s16, invis_bright) \
    X(s16, medpak_bright) \
    X(s16, inwep_bright) \
    X(s16, outwep_bright) \
    X(s16, inwep_no) \
    X(s16, outwep_no) \
    X(s16, remove_wepno) \
    X(u8, mpak_maxed) \
    X(u8, mpak_time) \
    X(u8, mpak_state) \
    X(u32, medpak_ID) \
    X(u32, wepfxID) \
    X(s16, wep_low) \
    X(u16, a1st_time) \
    X(u8, mp_equip_phase) \
    X(s16, chase_vel) \
    X(s16, swing_angle) \
    X(s16, gunratemul) \
    X(const AvpWeaponDef *, cur_def) \
    X(const AvpWepCmd *, wep_animptr) \
    X(const AvpWepCmd *, wait_ptr) \
    X(u8, cur_action) \
    X(s16, over_id) \
    X(s16, use1_id) \
    X(s16, use2_id) \
    X(u8, over_visible) \
    X(u8, use1_visible) \
    X(u8, use2_visible) \
    X(u8, cur_weps) \
    X(u8, old_weps) \
    X(s16, cur_wepno) \
    X(s16, new_wepno) \
    X(s16, medpak)

typedef struct AvpMpWeaponEngineState {
    uint32_t valid;
#define AVP_MP_DECLARE_WEAPON_FIELD(type, name) type name;
    AVP_MP_WEAPON_FIELDS(AVP_MP_DECLARE_WEAPON_FIELD)
#undef AVP_MP_DECLARE_WEAPON_FIELD
    AvpAmmoInfo ammo_info[6];
    unsigned pred_unlock_index;
} AvpMpWeaponEngineState;

/* These use the REAL original engine in this one process, serialized only.
 * Capture/restore never run weapon scripts or dispatch audio themselves.
 */
int avp_mp_weapon_native_valid(const AvpMpWeaponEngineState *source);
int avp_mp_weapon_native_capture(AvpMpWeaponEngineState *out);
int avp_mp_weapon_native_restore(const AvpMpWeaponEngineState *source);

typedef struct AvpMpWeaponScope {
    AvpMpWeaponEngineState original;
    int active;
} AvpMpWeaponScope;

int avp_mp_weapon_scope_enter(AvpMpWeaponScope *scope,
                               const AvpMpWeaponEngineState *weapon);
int avp_mp_weapon_scope_leave(AvpMpWeaponScope *scope,
                               AvpMpWeaponEngineState *weapon);

/* Feature-only original Predator availability private state bridge. */
unsigned avp_mp_weapons_native_get_index(void);
int avp_mp_weapons_native_set_index(unsigned index);

#endif
