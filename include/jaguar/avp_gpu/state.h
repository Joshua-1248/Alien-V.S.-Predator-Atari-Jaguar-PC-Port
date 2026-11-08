#ifndef AVP_JAGUAR_AVP_GPU_STATE_H
#define AVP_JAGUAR_AVP_GPU_STATE_H

#include "avp_types.h"
#include "amp.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/maze2_occlusion.h"
#include "jaguar/object_processor.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AVP-owned presentation state for the hosted Jaguar renderer.
 *
 * This state deliberately lives outside platform/pc.  It represents resources,
 * Object Processor state and presentation history consumed by AVP's Jaguar-side
 * renderer.  The PC host owns only the window/device used to present the final
 * framebuffer.
 */
/* Other participants, for rendering only. These are NOT AMP gameplay actors.
 * Use the original AAF sheets, projection, wall clipping and depth sorting.
 * A different-level actor is never submitted for visibility. */
typedef struct AvpGpuOtherPlayer {
    s32 x, y;
    u16 angle, creature, animation, frame;
} AvpGpuOtherPlayer;
#define AVP_GPU_OTHER_PLAYERS_MAX  (15u + 128u)
/* Local-multiplayer same-level player contacts for the automap and Marine
 * tracker. Never changes original automap reveal data or enemy AMP slots. */
typedef struct AvpGpuMapTeammate { u32 x, y; } AvpGpuMapTeammate;
#define AVP_GPU_MAP_TEAMMATES 15u

typedef struct AvpAvpGpuState {
    AvpGpuOtherPlayer other_players[AVP_GPU_OTHER_PLAYERS_MAX];
    unsigned other_player_count;
    /* Simulation contacts for the original Marine scanner (10 Hz). */
    AvpGpuMapTeammate map_teammates[AVP_GPU_MAP_TEAMMATES];
    /* Presentation-only positions for the Jaguar automap's moving dots.
     * Never feed these into scanner detection or gameplay decisions. */
    AvpGpuMapTeammate map_teammates_draw[AVP_GPU_MAP_TEAMMATES];
    unsigned map_teammate_count;
    unsigned char map_hide_teammates; /* Host map overlay only; never suppress Marine scanner contacts. */
    unsigned char hud_fixed_hue; /* PC Misc option: HUD ink hue independent of scene color. */
    int map_zoom_steps; /* 0 = Jaguar 1x, -8..8 host-only zoom steps */

    AvpGpuRgbImage panel_tex[74];
    /* LEVELS.S GetPanels: 13 base+notice composites, plus five numbered
     * sublevel variants. Original art is decoded from the user's ROM. */
    AvpGpuRgbImage notice_tex[18];
    u8 window_pairs[13][256];
    unsigned char window_pair_loaded[13];

    u8 *hud_base[3],*hud_energy[3],*hud_score_font[3];
    size_t hud_base_size[3],hud_energy_size[3],hud_score_font_size[3];
    u8 *hud_weapon_bar; /* FILES 204, original 48x8-pixel weapon-charge brush. */
    size_t hud_weapon_bar_size;
    u8 *hud_cocoon; /* FILES 197: 18 retail 32x32 cocoon animation frames. */
    size_t hud_cocoon_size;
    /* Scratch tiles let every HUD element use a single 320x228 coordinate plane. */
    u32 *hud_native_tiles;
    const u8 *hud_tiny_font;
    size_t hud_tiny_font_size;
    /* Marine motion-tracker brushes loaded by MAZESCRN.S::M_GetIm. */
    u8 *tracker_ring,*tracker_base1,*tracker_base2,*tracker_kill,*tracker_blips,*tracker_basex;
    size_t tracker_ring_size,tracker_base1_size,tracker_base2_size,tracker_kill_size,tracker_blips_size,tracker_basex_size;
    u8 hud_pal[3][32][3];
    u16 hud_pal_cry[3][32];
    /* HUD.S SetHUDPal applies the same brightness to base and dynamic ink. */
    u16 hud_active_pal[3][32];
    /* Host-resident Predator audio scope raster: original 43x45 HUD indices.
     * Its two bottom scan rows advance only on the original HP2 HUD event. */
    u8 predator_scope[43u*45u];
    unsigned char predator_scope_valid;
    unsigned char mp_waveform_enabled,mp_waveform_species;
    s16 mp_waveform_previous;
    u8 *qconv;
    size_t qconv_size;

    u8 *aaf[63];
    size_t aaf_size[63];
    unsigned aaf_loaded;

    u8 *fp_img[52];
    size_t fp_size[52];
    u8 fp_pal[52][208][3];
    unsigned char fp_pal_valid[52];
    int textures_loaded;

    AvpGpuRgbImage frontend_img[10][9];
    unsigned char frontend_loaded[10][9];
    u8 *mjp_font;
    size_t mjp_font_size;
    u8 *avp_font;
    size_t avp_font_size;
    unsigned frontend_mode,frontend_page;
    int frontend_title_choice,frontend_title_save;
    /* Host Object Processor title presentation only; Jaguar CPU remains untouched. */
    unsigned char frontend_title_splash;
    unsigned char frontend_title_reveal_active;
    unsigned char frontend_title_reveal_tick;
    /* PC presentation only: title-to-character-select video fade. */
    unsigned char frontend_title_depart_active;
    unsigned char frontend_title_depart_phase;
    unsigned char frontend_title_menu_dim_phase; /* retained title art, 16 down to 6 */
    unsigned char frontend_title_menu_pending;
    unsigned char frontend_credits_scrolling;
    unsigned frontend_credits_tick; /* hide options until overlay dim completes */
    int frontend_credits_y;          /* retail credits object Y above $4BE28 (CRSY) */
    unsigned frontend_credits_line;  /* first FILES 238 line in that object */
    int frontend_selected_species,frontend_select_detail;
    /* Retail SELECT object phrases: 3 authored pictures with independent
     * positions and scales, forwarded by MJP's CPU animation tables. */
    int frontend_select_pose_species[3];
    int frontend_select_pose_scale[3];
    int frontend_select_pose_x[3],frontend_select_pose_y[3];
    unsigned char frontend_select_pose_valid[3];
    unsigned char frontend_fx_kind;       /* 0 none, 1 CROSS, 2 FADEIN */
    unsigned char frontend_fx_active;
    unsigned char frontend_fx_phase;      /* original GPU blend phase 0..16 */
    unsigned char frontend_select_cross_tick; /* video-backed 50-VBL selection motion */
    unsigned char frontend_fx_from_role;
    unsigned char frontend_fx_to_role;
    /* Selected OP card bounds captured when the selection cross job starts.
     * The host must grow this object toward its original full-screen plane
     * while changing to the species detail picture. */
    int frontend_select_zoom_x,frontend_select_zoom_y;
    int frontend_select_zoom_w,frontend_select_zoom_h;
    unsigned char frontend_dim_active;
    unsigned char frontend_dim_phase;     /* held FADEIN result, normally 7 */
    /* MJP's eight One_Tick scene transition calls (MJPP -> MJPS).
     * Pure display state: gameplay, text and selection stay CPU-owned. */
    unsigned char frontend_page_cross_active;
    unsigned char frontend_page_cross_from,frontend_page_cross_to;
    unsigned char frontend_page_cross_phase;
    /* Retail MJP picture-object X ($12+4*D1), published by the CPU each
     * VBlank of $01DF2C; scene_blank marks a page closed onto $4BE18. */
    int frontend_picture_x;
    unsigned char frontend_scene_blank;
    unsigned frontend_transition_phase;
    unsigned frontend_transition_polls;
    unsigned char frontend_transition_active;
    unsigned char frontend_fame_species;
    unsigned char frontend_fame_active;
    unsigned char frontend_fame_editing;
    unsigned char frontend_fame_edit_slot,frontend_fame_edit_cursor;
    unsigned char frontend_fame_edit_character;
    unsigned char frontend_fame_edit_name[6];
    unsigned char frontend_fame_edit_name_valid;
    unsigned char frontend_text_large[12];
    unsigned char frontend_text_species[12];
    char frontend_text[12][96];
    unsigned frontend_text_x[12];
    /* Preserve exact Show_Tex per-glyph X from the original 68000.
       Its @ half-space cannot be represented by 7px string advance. */
    unsigned short frontend_text_glyph_x[12][96];
    /* Fame_Tex's score and six-character name occupy independent X columns.
     * Store the score separately rather than appending it to the name. */
    char frontend_fame_score[5][16];
    /* Fame's RDB $2A925 PRESS FIRE message is an independent small-font
     * overlay, never part of row zero of Fame_Tex's five records. */
    char frontend_fame_prompt[96];
    unsigned char frontend_fame_prompt_active;
    /* Hosted original-frame presentation: front-end fade/score roll are
     * advanced by MJP One_Tick, never by asynchronous X11 redraws. */
    unsigned short frontend_fame_intro_tick;
    unsigned char frontend_fame_depart_active;
    unsigned char frontend_fame_depart_phase;
    unsigned char frontend_terminated_transitions;
    unsigned char frontend_terminated_fade_active;
    unsigned char frontend_terminated_fade_phase;
    unsigned char frontend_terminated_fade_direction;
    /* PC presentation of the retail SELECT briefing. Text is decoded from
     * RDB.DTA by the host; the original MJP event order stays untouched. */
    char frontend_select_instructions[2][48];
    char frontend_select_brief[7][56];
    char frontend_select_status[32];
    /* The selection/briefing plane is revealed from original One_Tick events,
     * never host redraws. PRESS FIRE is independent of LOADING SIMULATION. */
    char frontend_select_fire_prompt[32];
    unsigned short frontend_select_visible_chars;
    unsigned short frontend_select_total_chars;
    unsigned short frontend_select_loading_ticks; /* post-briefing video interval */
    unsigned char frontend_select_fire_prompt_active;
    unsigned char frontend_select_brief_count;
    unsigned char frontend_select_brief_active;

    char hud_msg[3][96];
    unsigned hud_msg_x[3];
    unsigned hud_msg_valid;
    const u8 *hmsg_font;
    size_t hmsg_font_size;

    /* Retail COMPUTER.S presentation surface.  The CPU computer/menu state
       remains in game/computer.c; this is only the Jaguar-side 320x240 text
       and overlay plane presented while a terminal is active. */
    const u8 *computer_font[3];
    size_t computer_font_size[3];
    /* COMPUTER.S :: compbg_pal / compbg_cmp and the original cobj_tab.
       Indexed art is extracted only from the user's retail cartridge. */
    u8 *computer_bg_img;
    size_t computer_bg_img_size;
    u8 *computer_overlay_img[29]; /* FILES 104..132, inclusive */
    size_t computer_overlay_size[29];
    uint32_t computer_palette[256];
    unsigned char computer_palette_valid;
    /* Transparent CPU text plane, composited after the two object overlays. */
    uint32_t computer_pixels[320u*240u];
    unsigned char computer_active;
    int computer_obj1,computer_obj2;
    int computer_cursor;
    unsigned computer_menu_tag;
    int computer_menu_opts;

    AvpJaguarPauseOpState pause_op;

    AvpMaze2Occlusion maze_occlusion;

    /* GPU_AUTOMAP retail keeps a persistent map and adds newly traced maze
       geometry as the player moves.  This host-side discovery state is
       presentation-only; it never feeds collision or game logic. */
    u8 automap_seen[64u*64u];
    unsigned automap_level;
    /* MAZE5.GAS appends strokes directly to HUD.S::map_data on each GPU
       invocation.  Retain that PIXEL1 surface across later maze mutations. */
    u8 automap_bits[512u*64u];
    /* Per-observation scratch: only squares traced on this tick are painted. */
    u8 automap_touched[64u*64u];

    /* Previous authoritative 10 Hz state used only for presentation. */
    s32 amp_prev_x[AVP_NUM_AMPS],amp_prev_y[AVP_NUM_AMPS];
    u16 amp_prev_angle[AVP_NUM_AMPS];
    u32 amp_prev_generation[AVP_NUM_AMPS];
    s32 prev_wep_x,prev_wep_y;
    s16 prev_overlay_id;
    s16 prev_overlay_species;
    s16 prev_overlay_weapon;
    u8 prev_overlay_visible;
    s16 prev_swing_x,prev_swing_y;
    u8 prev_maze[64u*64u*8u];
    size_t prev_maze_size;
    int have_prev_sim;
    double render_alpha;
    int map_smooth_camera;
    int map_editor_world_only; /* PC Map Maker: native walls/flats, no gameplay overlays. */
    double map_editor_pitch; /* PC editor only, shifts projection horizon */
    int host_continuous_heading; /* PC enhancement only; retail rendering remains integer */
    int host_widescreen_fov; /* Host-only: additional 3D columns, never a gameplay/ROM rule. */
    int host_extend_draw_distance; /* Host-only: 0=8, 1=16, 2=24, 3=32, 4=whole map. */
    int host_extend_fog_distance; /* Host-only: 0=original, 1=2x, 2=3x, 3=4x. */
    int host_disable_fog; /* Host-only: bypass distance CRY darkening, not geometry. */
} AvpAvpGpuState;

void avp_gpu_state_init(AvpAvpGpuState *state);
void avp_gpu_state_destroy(AvpAvpGpuState *state);
/* HUD.S::ResetMap clears exploration, without toggling map_on. */
/* MAZE/PLAYER.INC: map_data is a persistent 512x512 PIXEL1 surface.
 * MAP_WMAX=128 describes the destination window, not the stored map. */
#define AVP_AUTOMAP_PIXELS 512u
#define AVP_AUTOMAP_ROW_BYTES (AVP_AUTOMAP_PIXELS/8u)
#define AVP_AUTOMAP_RASTER_BYTES (AVP_AUTOMAP_PIXELS*AVP_AUTOMAP_ROW_BYTES)
/* Computes the MAP5.GAS fixed-point source coordinates for one HUD pixel.
 * Inputs are original 16.16 player position and 1.15 sine/cosine values. */
void avp_gpu_automap_sample(u32 xpos,u32 ypos,s32 sine,s32 cosine,
                             unsigned width,unsigned height,int pixel_x,int pixel_y,
                             int *src_x,int *src_y);
void avp_gpu_automap_rasterize(const AvpAvpGpuState *state,
                               const u8 *maze,u16 mw,u16 mh,
                               u8 pixels[AVP_AUTOMAP_RASTER_BYTES]);
void avp_gpu_automap_reset(AvpAvpGpuState *state);
void avp_gpu_automap_reveal_all(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh,unsigned logical_level);
void avp_gpu_automap_observe(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh,
                             u32 xpos,u32 ypos,u32 angle,unsigned logical_level);
/* Both discovery and the original one-bit history survive a quickload.
 * This is visual state only, separate from retail collision/gameplay data. */
typedef struct AvpGpuAutomapSnapshot {
    u8 seen[64u*64u];
    unsigned level;
    u8 bits[AVP_AUTOMAP_RASTER_BYTES];
} AvpGpuAutomapSnapshot;
void avp_gpu_automap_save(const AvpAvpGpuState *state,AvpGpuAutomapSnapshot *dst);
void avp_gpu_automap_load(AvpAvpGpuState *state,const AvpGpuAutomapSnapshot *src);
/* PC-only option: preserve only discovered cells and the one-bit strokes
 * already authored by the retail automap, separately for each level.
 * This archive belongs to a player/view, never to a shared world. */
#define AVP_AUTOMAP_HISTORY_LEVELS 15u
typedef struct AvpGpuAutomapHistory {
    AvpGpuAutomapSnapshot *level[AVP_AUTOMAP_HISTORY_LEVELS];
} AvpGpuAutomapHistory;
void avp_gpu_automap_history_capture(AvpGpuAutomapHistory *history,
                                     const AvpAvpGpuState *state);
void avp_gpu_automap_history_enter(AvpGpuAutomapHistory *history,
                                   AvpAvpGpuState *state,unsigned level,int enabled);
void avp_gpu_automap_history_destroy(AvpGpuAutomapHistory *history);


#ifdef __cplusplus
}
#endif

#endif
