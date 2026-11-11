#ifndef AVP_PC_VIDEO_H
#define AVP_PC_VIDEO_H
#include "avp_types.h"
#include <stddef.h>
#include "jaguar/avp_gpu/state.h"
#include "pc/pc_platform.h"

typedef struct AvpPcVideo AvpPcVideo;

typedef enum AvpPcVideoAction {
    AVP_PC_ACTION_NONE=0,
    AVP_PC_ACTION_RESTORE_HEALTH=1u<<0,
    AVP_PC_ACTION_RESTORE_AMMO=1u<<1,
    AVP_PC_ACTION_GIVE_WEAPONS=1u<<2,
    AVP_PC_ACTION_QUICKSAVE=1u<<3,
    AVP_PC_ACTION_QUICKLOAD=1u<<4,
    AVP_PC_ACTION_SECURITY_UP=1u<<6,
    AVP_PC_ACTION_SECURITY_DOWN=1u<<7,
    AVP_PC_ACTION_KILL_SELECTED=1u<<8,
    AVP_PC_ACTION_KILL_ALL=1u<<9,
    AVP_PC_ACTION_RESTART_TITLE=1u<<10,
    AVP_PC_ACTION_ORIG_TRACKER_TOGGLE=1u<<11,
    AVP_PC_ACTION_ORIG_WEAPON1=1u<<12,
    AVP_PC_ACTION_ORIG_WEAPON2=1u<<13,
    AVP_PC_ACTION_ORIG_WEAPON3=1u<<14,
    AVP_PC_ACTION_ORIG_WEAPON4=1u<<15,
    AVP_PC_ACTION_ORIG_LEVEL_NEXT=1u<<16,
    AVP_PC_ACTION_ORIG_LEVEL_PREVIOUS=1u<<17,
    AVP_PC_ACTION_CLEAR_ENEMIES=1u<<18,
    AVP_PC_ACTION_CLEAR_HAZARDS=1u<<19,
    AVP_PC_ACTION_REFRESH_LEVEL=1u<<20,
    AVP_PC_ACTION_CUSTOM_MAP_START=1u<<21
} AvpPcVideoAction;
AvpPcVideo *avp_pc_video_create(unsigned width,unsigned height,const char *title);
void avp_pc_video_destroy(AvpPcVideo *v);
/* Optional original-ROM title graphics. No-op when reference model disabled. */
void avp_pc_video_title_native_begin(AvpPcVideo *v,const AvpRomImage *rom);
void avp_pc_video_title_native_boot_field(AvpPcVideo *v);
void avp_pc_video_title_native_replay_begin(AvpPcVideo *v,const AvpRomImage *rom);
/* Register an immutable copy of a retail maze for the PC Map Maker.
 * It never rebinds, patches, or applies map bytes to the live game. */
void avp_pc_video_map_maker_bind(AvpPcVideo *v,unsigned level,const u8 *decoded,size_t bytes);
void avp_pc_video_map_maker_rom_path(AvpPcVideo *v,const char *path);
/* Resolve only validated .avpm/.avpo stems beside the executable. */
int avp_pc_video_custom_map_path(const char *stem,const char *extension,char *out,size_t cap);
int avp_pc_video_poll(AvpPcVideo *v,u32 *active_low_pad,int *quit);
void avp_pc_video_set_frontend_input(AvpPcVideo *v,int active);
int avp_pc_video_alien_use_down(const AvpPcVideo *v);
int avp_pc_video_alien_use_separate(const AvpPcVideo *v);
int avp_pc_video_take_alien_use(AvpPcVideo *v);
int avp_pc_video_buffer_short_presses(const AvpPcVideo *v);
int avp_pc_video_buffer_short_analog(const AvpPcVideo *v);
u32 avp_pc_video_simulation_pad(AvpPcVideo *v,u32 current);
int avp_pc_video_noclip(const AvpPcVideo *v);
int avp_pc_video_smooth_presentation(const AvpPcVideo *v);
/* PC-only output raster cadence; does not drive 60 Hz Jaguar VBL or gameplay. */
unsigned avp_pc_video_presentation_hz(const AvpPcVideo *v);
/* -1 left, +1 right, 0 neutral; live bound keyboard keys only. */
/* Independent PC-action Q/E sidestep while A/D turns; zero in Jaguar mode. */
int avp_pc_video_keyboard_strafe_during_turn(const AvpPcVideo *v);
int avp_pc_video_keyboard_turn_direction(const AvpPcVideo *v);
int avp_pc_video_menu_open(const AvpPcVideo *v);
int avp_pc_video_full_cpu_systems(const AvpPcVideo *v);
/* F12 > Miscellaneous; applies on a new Marine start, not a loaded save. */
int avp_pc_video_marine_start_shotgun(const AvpPcVideo *v);
int avp_pc_video_debug_immediate_in_game(const AvpPcVideo *v);
unsigned avp_pc_video_take_actions(AvpPcVideo *v);
int avp_pc_video_cheat_custom_map(const AvpPcVideo *v,char *out,size_t cap);
/* One-shot F12 > Miscellaneous > Restart Game, including during co-op. */
int avp_pc_video_take_restart_title(AvpPcVideo *v);
/* Require a fresh frontend confirmation after leaving gameplay. */
void avp_pc_video_prepare_title_return(AvpPcVideo *v);
/* Consume the debug refresh request WITHOUT consuming unrelated actions. */
int avp_pc_video_take_refresh_level(AvpPcVideo *v);
#ifdef AVP_LOCAL_MULTIPLAYER
unsigned avp_pc_video_take_local_mp_cheat_actions(AvpPcVideo *v,unsigned player);
#endif
/* PC-only 180-degree clockwise Quick Turn [Extra] press edges. */
unsigned avp_pc_video_take_quick_turn(AvpPcVideo *v);
int avp_pc_video_quick_turn_allowed(const AvpPcVideo *v);
int avp_pc_video_take_weapon_delta(AvpPcVideo *v);
int avp_pc_video_infinite_health(const AvpPcVideo *v);
int avp_pc_video_infinite_ammo(const AvpPcVideo *v);
int avp_pc_video_freeze_enemies(const AvpPcVideo *v);
int avp_pc_video_attacks_disabled(const AvpPcVideo *v);
int avp_pc_video_marine_pulse_los_hits(const AvpPcVideo *v);
int avp_pc_video_unlimited_shooting_range(const AvpPcVideo *v);
int avp_pc_video_marine_fire_interrupt(const AvpPcVideo *v);
int avp_pc_video_normalize_diagonal_contact_range(const AvpPcVideo *v);
unsigned avp_pc_video_enemy_activation_distance(const AvpPcVideo *v);
int avp_pc_video_facehug_attach_disabled(const AvpPcVideo *v);
int avp_pc_video_kill_selected_type(const AvpPcVideo *v);
/* Original retail cheat modes are session-only, off by default, never saved.
 * New/legacy F12 cheats keep their own independent state. */
int avp_pc_video_original_god(const AvpPcVideo *v);
int avp_pc_video_original_god_player(const AvpPcVideo *v,unsigned player);
int avp_pc_video_original_positioning_player(const AvpPcVideo *v,unsigned player);
int avp_pc_video_original_positioning(const AvpPcVideo *v);
void avp_pc_video_original_reset(AvpPcVideo *v);
void avp_pc_video_original_gps_actor(AvpPcVideo *v,unsigned player,
    unsigned level,u32 x,u32 y);
void avp_pc_video_original_actor_status(AvpPcVideo *v,unsigned player,
    unsigned species,unsigned weapons,int tracker);
int avp_pc_video_tracker_cheat(const AvpPcVideo *v);
int avp_pc_video_tracker_sounds_enabled(const AvpPcVideo *v);
int avp_pc_video_player_pain_sounds_enabled(const AvpPcVideo *v);
int avp_pc_video_facehug_pain_sounds_enabled(const AvpPcVideo *v);
int avp_pc_video_facehug_shake_sounds_enabled(const AvpPcVideo *v);
float avp_pc_video_modern_turn(AvpPcVideo *v);
int avp_pc_video_aim_enabled(const AvpPcVideo *v);
float avp_pc_video_aim_sensitivity(const AvpPcVideo *v);
float avp_pc_video_aim_speed(const AvpPcVideo *v);
void avp_pc_video_analog_motion(const AvpPcVideo *v,float *forward,float *strafe);
int avp_pc_video_analog_enabled(const AvpPcVideo *v);
int avp_pc_video_enhanced_analog_enabled(const AvpPcVideo *v);
/* Per-host-poll axis sample for Enhanced Analog Controls. */
int avp_pc_video_live_stick_turn(const AvpPcVideo *v,float *axis);
int avp_pc_video_mouse_aim_active(const AvpPcVideo *v);
float avp_pc_video_turn_sensitivity(const AvpPcVideo *v);
float avp_pc_video_turn_speed(const AvpPcVideo *v);
int avp_pc_video_take_stage_delta(AvpPcVideo *v);
int avp_pc_video_take_species_request(AvpPcVideo *v);
int avp_pc_video_take_hz_delta(AvpPcVideo *v);
void avp_pc_video_snapshot_sim(AvpPcVideo *v,const u8 *maze,u16 mw,u16 mh);
void avp_pc_video_set_render_alpha(AvpPcVideo *v,double alpha);
int avp_pc_video_load_retail_textures(AvpPcVideo *v,const AvpRomImage *rom);
int avp_pc_video_frontend_load(AvpPcVideo *v,const AvpRomImage *rom,unsigned mode,unsigned file_index,unsigned role);
void avp_pc_video_frontend_event(AvpPcVideo *v,unsigned event,s32 arg0,s32 arg1);
int avp_pc_video_computer_active(const AvpPcVideo *v);
void avp_pc_video_computer_draw_glyph(AvpPcVideo *v,unsigned brush,unsigned source_x,
    unsigned width,unsigned x,unsigned y,int clear);
void avp_pc_video_computer_draw_retail_menu(AvpPcVideo *v,const char *record,u32 source_address);
void avp_pc_video_computer_draw_retail_overlay(AvpPcVideo *v,const char *record,u32 source_address);
int avp_pc_video_frontend_transition_done(AvpPcVideo *v,unsigned phase);
void avp_pc_video_frontend_draw_text(AvpPcVideo *v,unsigned line,const char *text,unsigned x);
void avp_pc_video_frontend_select_instructions(AvpPcVideo *v,const char *rotate,const char *fire);
void avp_pc_video_frontend_select_briefing(AvpPcVideo *v,const char *lines,const char *status);
void avp_pc_video_frontend_select_prompt(AvpPcVideo *v,const char *prompt);
int avp_pc_video_frontend_select_loading_ready(const AvpPcVideo *v);
void avp_pc_video_hud_message_line(AvpPcVideo *v,unsigned line,const char *text,unsigned x_pixels);
void avp_pc_video_hud_message_clear(AvpPcVideo *v);
void avp_pc_video_object_event(AvpPcVideo *v,unsigned event,u32 arg0,u32 arg1,u32 arg2);
int avp_pc_video_pause_active(const AvpPcVideo *v);
void avp_pc_video_present_pause(AvpPcVideo *v);
void avp_pc_video_present_gameplay(AvpPcVideo *v);
#ifdef AVP_LOCAL_MULTIPLAYER
#include "local_mp_worlds.h"
#include "local_mp_input.h"
#include "local_mp.h"
/* Host-only F2 Start Game request: read-only for the title loop and taken
 * once by the PC executable after the original menu has exited. */
/* Called during COMPUTER.S's synchronous co-op modal session.
 * Only the activating player drives the original Jaguar computer menu. */
int avp_pc_video_local_mp_terminal_poll(AvpPcVideo *v,unsigned owner,
                                       unsigned players,u32 *pad,int *quit);
/* S116: Snapshot the last co-op composite, then display the unchanged original
 * computer raster inside ONLY the terminal owner's split-screen rectangle.
 * All other actors' last views remain visible, frozen as gameplay pauses. */
int avp_pc_video_local_mp_terminal_begin(AvpPcVideo *v,unsigned owner,unsigned players);
void avp_pc_video_local_mp_terminal_end(AvpPcVideo *v);
int avp_pc_video_local_mp_menu_open(const AvpPcVideo *v);
/* Persistent F2 visibility probe for the title's attract timer. The normal
 * polling-only menu-open query above must keep its existing semantics. */
int avp_pc_video_title_mp_menu_open(const AvpPcVideo *v);
int avp_pc_video_local_mp_launch_pending(const AvpPcVideo *v);
int avp_pc_video_take_local_mp_launch(AvpPcVideo *v,AvpLocalMpSetup *out);
void avp_pc_video_local_mp_select_custom(AvpPcVideo *v,const char *stem);
/* Explicit opt-in native movement/view smoke, not co-op gameplay. */
int avp_pc_video_local_mp_walktest_mode(AvpPcVideo *v,int enable);
const AvpMpInputConfig *avp_pc_video_local_mp_profiles(const AvpPcVideo *v);
const AvpLocalMpSetup *avp_pc_video_local_mp_setup(const AvpPcVideo *v);
void avp_pc_video_local_mp_render_alpha(AvpPcVideo *v,double alpha);
void avp_pc_video_local_mp_runtime_setup(AvpPcVideo *v,const AvpLocalMpSetup *setup);
/* Collect separately bound keyboard and physically identified mouse commands
 * for the 10-Hz co-op scheduler. Does NOT start multiplayer, alter the legacy
 * single-player joystick path or synthesize any game actors. */
int avp_pc_video_local_mp_live_turn(AvpPcVideo *v,unsigned player,float *axis);
int avp_pc_video_local_mp_take_commands(AvpPcVideo *v, unsigned players,
    AvpMpPlayerCommand out[AVP_LOCAL_MP_MAX_PLAYERS]);
/* Bots are simulation participants, not host input devices. They may run
 * without keyboard/window focus, but not while a gameplay menu is open. */
int avp_pc_video_local_mp_bots_may_think(const AvpPcVideo *v);
/* Present ALL separately rendered player frames in the existing PC window.
 * Returns 0 if the session cannot produce all independent frames. Never
 * creates windows, a second renderer process, or new gameplay state. */
void avp_pc_video_local_mp_set_paused(AvpPcVideo *v,int paused);
/* Presentation-only score/time overlay; empty string disables it. */
void avp_pc_video_local_mp_deathmatch_status(AvpPcVideo *v,const char *status);
unsigned avp_pc_video_present_local_mp(AvpPcVideo *v, AvpMpSession *session);
/* Render the ACTUAL existing Jaguar-derived software renderer into a distinct
 * full HUD+world pixel buffer for this actor. Requires native world/actor
 * scope already bound. All ROM images are borrowed from the live GPU assets;
 * per-view presentation state and output pixels are independent. This is a
 * renderer API, not an enabled multiplayer game loop or HUD/audio isolation. */
/* Route the original HUD_MSG.S draw/clear callbacks to the actor currently
 * bound in the native 10-Hz simulation; -1 restores legacy host routing. */
int avp_pc_video_local_mp_hud_message_route(AvpPcVideo *v,int player);
/* Read-only verification of each viewport's original message brush. */
int avp_pc_video_local_mp_hud_message_copy(const AvpPcVideo *v,unsigned player,
    unsigned line,char *out,size_t capacity);
/* PC Deathmatch-only two-line notice, drawn after per-player death fades. */
int avp_pc_video_local_mp_draw_kill_notice(AvpPcVideo *v,unsigned player,
    unsigned kind,unsigned other_player,int viewing_type);
int avp_pc_video_local_mp_set_other_players(AvpPcVideo *v,unsigned viewer,
    const AvpGpuOtherPlayer *actors,unsigned count);
int avp_pc_video_local_mp_render_actor(AvpPcVideo *v,unsigned player,
    const u8 *maze,u16 width,u16 height,u32 xpos,u32 ypos,u32 angle,
    unsigned level,u64 vblank,AvpLocalMpSourceFrame *out);
int avp_pc_video_local_mp_waveform_step(AvpPcVideo *v,unsigned player,s32 left,s32 right);
int avp_pc_video_local_mp_map_teammates(AvpPcVideo *v,unsigned player,
    const AvpGpuMapTeammate *teammates,
    const AvpGpuMapTeammate *presentation,unsigned count,unsigned zoom_steps);
int avp_pc_video_local_mp_automap_observe(AvpPcVideo *v,unsigned player,
    const u8 *maze,u16 width,u16 height,u32 xpos,u32 ypos,u32 angle,unsigned level);
int avp_pc_video_local_mp_snapshot_actor(AvpPcVideo *v,unsigned player,
    const u8 *maze,u16 width,u16 height);
#endif
/* Host display-only bridge for MAZE.S duct X_FADE, not a gameplay effect. */
int avp_pc_video_begin_gameplay_fade(AvpPcVideo *v);
void avp_pc_video_end_gameplay_fade(AvpPcVideo *v);
void avp_pc_video_present_boot_black(AvpPcVideo *v);
void avp_pc_video_reset_presentation(AvpPcVideo *v);
void avp_pc_video_reset_automap(AvpPcVideo *v);
void avp_pc_video_automap_observe(AvpPcVideo *v,const u8 *maze,u16 mw,u16 mh,
                                   u32 xpos,u32 ypos,u32 angle,unsigned logical_level);
void avp_pc_video_automap_save(const AvpPcVideo *v,AvpGpuAutomapSnapshot *dst);
void avp_pc_video_automap_load(AvpPcVideo *v,const AvpGpuAutomapSnapshot *src);
void avp_pc_video_frontend_tick(AvpPcVideo *v,unsigned mode,s32 choice);
void avp_pc_video_render_maze(AvpPcVideo *v,const u8 *maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,int rom_ok,unsigned logical_level,u64 sim_ticks,unsigned simulation_hz);
/* Applies F12 host audio options after FullSynth startup. */
void avp_pc_video_audio_apply_settings(const AvpPcVideo *v);
#endif
