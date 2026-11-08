#ifndef AVP_JAGUAR_AVP_GPU_SOFTWARE_RENDERER_H
#define AVP_JAGUAR_AVP_GPU_SOFTWARE_RENDERER_H

#include "avp_types.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void avp_gpu_snapshot_sim(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh);
void avp_gpu_set_render_alpha(AvpAvpGpuState *state,double alpha);
/* Borrowed original ROM AAF animation assets; no generated textures.
 * This is per-viewport presentation data, not original gameplay state. */
int avp_gpu_set_other_players(AvpAvpGpuState *state,
                               const AvpGpuOtherPlayer *actors,unsigned count);
/* Jaguar HUD.S::HP2 event: two new sound-scope samples per game update.
 * The host renderer retains the previous scan rows across presented frames. */
/* Per-view local multiplayer visual contacts, independent of gameplay. */
void avp_gpu_show_map_teammates(AvpAvpGpuState *state,int enabled);
int avp_gpu_set_map_teammates(AvpAvpGpuState *state,
    const AvpGpuMapTeammate *players,unsigned count);
/* A matching visual roster for the map ONLY; original scanner remains on
 * map_teammates. Invalid counts leave both copies untouched. */
int avp_gpu_set_map_teammates_draw(AvpAvpGpuState *state,
    const AvpGpuMapTeammate *players,unsigned count);
void avp_gpu_set_map_zoom(AvpAvpGpuState *state,int steps);
void avp_gpu_predator_scope_step(AvpAvpGpuState *state,s16 previous,s16 midpoint,s16 next);
/* MAZE4 tracker distance must be available before HUD.S::TC makes the
 * sound decision at the authoritative 10 Hz update, not after a PC render. */
u32 avp_gpu_tracker_nearest_contact(u32 xpos,u32 ypos,u32 angle);
/* Same MAZE4 scanner with living same-level multiplayer player contacts. */
u32 avp_gpu_tracker_nearest_contact_for_view(const AvpAvpGpuState *state,
    u32 xpos,u32 ypos,u32 angle);

/* PC multiplayer overlay only; uses the original Jaguar HUD message font. */
void avp_gpu_draw_dm_kill_notice(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                 unsigned kind,unsigned other_player,
                                 int viewing_type);
void avp_gpu_hud_message_clear(AvpAvpGpuState *state);
void avp_gpu_hud_message_line(AvpAvpGpuState *state,unsigned line,const char *text,unsigned x_pixels);
void avp_gpu_computer_event(AvpAvpGpuState *state,unsigned event,s32 arg0,s32 arg1);
void avp_gpu_computer_draw_glyph(AvpAvpGpuState *state,unsigned brush,unsigned source_x,
    unsigned width,unsigned x,unsigned y,int clear);
void avp_gpu_computer_draw_retail_menu(AvpAvpGpuState *state,const char *record,u32 source_address);
/* FONT.S coordinate records can be layered without an intervening CLS. */
void avp_gpu_computer_draw_retail_overlay(AvpAvpGpuState *state,const char *record,u32 source_address);
void avp_gpu_computer_present(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb);
void avp_gpu_render_maze(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    const u8 *maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,
    int rom_ok,unsigned logical_level,u64 vblank_counter,
    int smooth_presentation,int debug_immediate);

#ifdef __cplusplus
}
#endif

#endif
