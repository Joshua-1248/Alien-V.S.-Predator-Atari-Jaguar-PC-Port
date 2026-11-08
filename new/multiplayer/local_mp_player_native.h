#ifndef AVP_NEW_LOCAL_MP_PLAYER_NATIVE_H
#define AVP_NEW_LOCAL_MP_PLAYER_NATIVE_H

#include "player.h"
#include "maze.h"
#include "weapons.h"
#include "joypad.h"
#include <stdint.h>

/* Optional PC-only capture of the ORIGINAL PLAYER.S C reconstruction.
 * The data fields listed here are owned by player.c, except for explicitly
 * enumerated player-facing maze coordinates, weapon selectors and joy input.
 * This is NOT the complete multiworld simulation state. Other modules own
 * player HUD, weapon-script execution, inventory, AMP targets, audio, etc.
 * One thread only, no reentrant gameplay while temporarily bound.
 */
#define AVP_MP_PLAYER_FIELDS(X) \
    X(u16, seed0) \
    X(u16, seed1) \
    X(s32, x_vel) \
    X(s32, y_vel) \
    X(s16, ang_vel) \
    X(s16, ang_acc) \
    X(s16, max_avel) \
    X(s16, acc_shift) \
    X(s16, dec_shift) \
    X(u32, max_speed) \
    X(u16, player_vel) \
    X(s16, player_type) \
    X(s16, player_energy) \
    X(s16, max_energy) \
    X(s16, old_energy) \
    X(s16, pain) \
    X(s16, player_dead) \
    X(s16, game_over) \
    X(s16, key_lock) \
    X(s16, destruct_flag) \
    X(s16, launch_flag) \
    X(s32, score) \
    X(u32, pain_cols) \
    X(u32, new_pos) \
    X(u32, old_pos) \
    X(s16, cheat) \
    X(u8, wep_fire) \
    X(u8, one_fire) \
    X(u8, plreset_count) \
    X(u16, fade_c) \
    X(u16, xtra_c) \
    X(u16, invisflag) \
    X(u16, pred_vision_vmode) \
    X(u8, invis_act) \
    X(u8, invis_stat) \
    X(u8, medpak_act) \
    X(u16, predo) \
    X(u16, predo_count) \
    X(u32, predo_ID) \
    X(u32, bg_fx) \
    X(s16, bg_repeat) \
    X(s16, bg_delay) \
    X(s16, bg_count) \
    X(s16, last_bg) \
    X(u8, allow_lcs) \
    X(u8, allow_god) \
    X(u32, last_moan) \
    X(int, host_analog_motion) \
    X(float, host_analog_forward) \
    X(float, host_analog_strafe) \
    X(int, host_keyboard_strafe_during_turn) \
    X(int, host_analog_aim) \
    X(float, host_aim_turn) \
    X(float, host_aim_sensitivity) \
    X(float, host_aim_speed) \
    X(float, host_digital_turn_sensitivity) \
    X(float, host_digital_turn_speed) \
    X(int, host_enhanced_analog_aim) \
    X(int, host_mouse_aim) \
    X(int, host_live_heading_enabled) \
    X(u32, host_live_heading_value) \
    X(int, host_quick_turn_enabled) \
    X(u32, host_quick_turn_heading) \
    X(int, host_quick_turn_preserve_heading) \
    X(s32, spark_x) \
    X(s32, spark_y) \
    X(u32, shot_distance) \
    X(AvpVoidFn, init_move) \
    X(AvpVoidFn, reset_move) \
    X(AvpVoidFn, do_move) \
    X(AvpVoidFn, x_read) \
    X(u32, x_pos) \
    X(u32, y_pos) \
    X(u32, centre_angle) \
    X(s32, sin_ang) \
    X(s32, cos_ang) \
    X(u8, cur_weps) \
    X(u8, old_weps) \
    X(s16, cur_wepno) \
    X(s16, new_wepno) \
    X(u32, joy_cur) \
    X(u32, joy_edge)

typedef struct AvpMpPlayerEngineState {
    uint32_t valid;
#define AVP_MP_DECLARE_PLAYER_FIELD(type, name) type name;
    AVP_MP_PLAYER_FIELDS(AVP_MP_DECLARE_PLAYER_FIELD)
#undef AVP_MP_DECLARE_PLAYER_FIELD
    /* Snapshot the three meaningful fields, not padding, of the host
       analog-look accumulator to keep GCC/Clang captures deterministic. */
    double continuous_heading;
    uint32_t continuous_published;
    int continuous_active;
} AvpMpPlayerEngineState;

int avp_mp_player_native_valid(const AvpMpPlayerEngineState *source);
int avp_mp_player_native_capture(AvpMpPlayerEngineState *out);
int avp_mp_player_native_restore(const AvpMpPlayerEngineState *source);

/* Exact original PLAYER.S actor body, with pre-sampled player-owned Jaguar
 * joy_cur/joy_edge. DoorKeys/Collectables/weapons remain original; timed doors
 * are deferred to the one-per-world callback. No alternative physics/AI. */
void avp_mp_player_native_step_actor(void);
void avp_mp_player_native_step_world_doors(void);


typedef struct AvpMpPlayerScope {
    AvpMpPlayerEngineState original;
    int active;
} AvpMpPlayerScope;

int avp_mp_player_scope_enter(AvpMpPlayerScope *scope,
                              const AvpMpPlayerEngineState *player);
int avp_mp_player_scope_leave(AvpMpPlayerScope *scope,
                              AvpMpPlayerEngineState *player);

#endif
