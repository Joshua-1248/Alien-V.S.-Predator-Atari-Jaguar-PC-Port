#ifndef AVP_LOCAL_MP_HUDVIEW_NATIVE_H
#define AVP_LOCAL_MP_HUDVIEW_NATIVE_H
#include "hud.h"
#include <stdint.h>

/* Player-specific visual HUD.S state. Countdown/global cocoon progression,
 * timer source, drawing callbacks and audio output are intentionally NOT
 * claimed as isolated by this bridge. */
#define AVP_MP_HUDVIEW_MAGIC UINT32_C(0x48565731)
typedef struct AvpMpHudViewState {
    uint32_t valid;
    u8 show_coords, map_on;
    s16 show_mt; /* actual player-owned original motion tracker pickup */
    s16 use_cocoon, num_cocoons;
    u32 ccn_xsave, ccn_ysave;
    AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
    s16 old_meter, scope_frame, pred_meter_previous;
    u32 tracker_nearest;
    s32 pred_meter_sample_a, pred_meter_sample_b;
    s16 nrg_x,nrg_y,nrg_barwidth,nrg_width,nrg_height,nrg_nframes,nrg_frame;
    u16 nrg_rescale,nrg_size;
    s8 hud_bright;
    s16 nrg_ftime,nrg_flash,nrg_draw_frame;
    u16 nrg_draw_pixels;
    u32 click_track;
    AvpTrackerAudioState tracker_audio_state;
    s16 pred_meter_left,pred_meter_right;
    u8 old_map;
    AvpMapInfo map_info;
} AvpMpHudViewState;
/* Level-owned original Countdown/clock state; keep one clock per shared
 * level, never increment it separately for each co-op player. */
#define AVP_MP_HUDWORLD_MAGIC UINT32_C(0x48574431)
typedef struct AvpMpHudWorldState {
    uint32_t valid;
    s16 counttime, counting;
    u32 ticktime;
    s8 flash_dir;
    u32 frames_now, one_sec;
} AvpMpHudWorldState;
int avp_mp_hudworld_native_valid(const AvpMpHudWorldState *in);
int avp_mp_hudworld_native_capture(AvpMpHudWorldState *out);
int avp_mp_hudworld_native_restore(const AvpMpHudWorldState *in);
int avp_mp_hudview_native_valid(const AvpMpHudViewState *in);
int avp_mp_hudview_native_capture(AvpMpHudViewState *out);
int avp_mp_hudview_native_restore(const AvpMpHudViewState *in);
#endif
