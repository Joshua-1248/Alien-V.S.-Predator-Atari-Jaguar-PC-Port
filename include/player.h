#ifndef AVP_PLAYER_H
#define AVP_PLAYER_H
#include "avp_types.h"

enum AvpPlayerType { PT_HUMAN=0, PT_ALIEN=4, PT_PREDATOR=8 };

typedef void (*AvpVoidFn)(void);

extern volatile u16 seed0, seed1;
extern s32 x_vel, y_vel;
extern s16 ang_vel, ang_acc, max_avel, acc_shift, dec_shift;
/* PC Enhanced Analog Controls only: host-poll-rate heading at game tick. */
void avp_player_host_live_heading(int enabled, u32 heading);
/* Host-only PC Quick Turn animation; leaves Jaguar input mapping untouched. */
void avp_player_host_quick_turn_heading(int enabled, u32 heading);
void avp_player_host_quick_turn_reset(void);
extern u32 max_speed;
extern u16 player_vel;
extern s16 player_type, player_energy, max_energy, old_energy, pain;
extern s16 player_dead, game_over, key_lock, destruct_flag, launch_flag;
extern s32 score;
extern s16 cheat;
extern u32 pain_cols;
extern u32 new_pos, old_pos;
extern u8 wep_fire, one_fire, plreset_count;
extern u16 fade_c, xtra_c, invisflag;
extern u16 pred_vision_vmode;
extern u8 invis_act, invis_stat, medpak_act;
extern u16 predo, predo_count;
extern u32 bg_fx;
extern s16 bg_repeat, bg_delay, bg_count, last_bg;

extern AvpVoidFn init_move, reset_move, do_move, x_read;

void StdController(void);
void InitPlayer(void);
void ResetPlayer(void);
void init_random(void);
u16 avp_random(void);
void BGSounds(void);
void UpdatePlayer(void);
void ResetMove(void);
void InitMove(void);
void MovePlayer(void);
/* Optional host analog movement and horizontal aiming; Jaguar digital control is unchanged. */
void avp_player_host_analog_motion(int enabled,float forward,float strafe);
/* Only for simultaneous PC-action keyboard sidestep plus turn. */
void avp_player_host_keyboard_strafe_during_turn(int direction);
void avp_player_host_analog_aim(int enabled,float turn,float sensitivity,float speed);
void avp_player_host_aim_options(int enhanced_analog,int mouse_mode);
void TidyMove(void);
void PlayerWeapons(void);
void do_predo(void);
void MapKeys(void);
void ChangeLevel(void);
void ExtraKeys(void);
void TestKeys(void);
void HumanPain(void);
void Pain(void);
/* Host-only audio preference: does not change the Jaguar pain state or RNG. */
void avp_player_set_pain_sounds(int enabled);
/* Per-bound-player co-op visual cue, not a gameplay state or SFX replacement. */
void avp_player_clear_last_pain_sfx(void);
unsigned avp_player_last_pain_sfx(void);
void avp_player_set_facehug_pain_sounds(int enabled);
struct AvpAmp; struct AvpAmp *TestSpark(void);
/* Optional PC deathmatch shot test. Invoked only after the original
 * PLAYER.S weapon trace and AMP closest-hit comparison; never installed in
 * single-player or co-op. Returns 1 when a closer player took the shot. */
typedef int (*AvpPlayerMpShotHook)(void *user,s32 origin_x,s32 origin_y,
        s32 boxx,s32 boxy,s16 cs,s16 sn,u16 width,u32 closest,s16 damage);
void avp_player_set_mp_shot_hook(AvpPlayerMpShotHook callback,void *user);
extern u8 map_on,allow_lcs,allow_god;
#endif
