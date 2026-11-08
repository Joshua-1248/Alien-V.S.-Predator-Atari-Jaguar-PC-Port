/* End-of-file optional hook in the original HUD.S C reconstruction. */
#include "../../new/multiplayer/local_mp_hudview_native.h"
#include <string.h>
int avp_mp_hudview_native_valid(const AvpMpHudViewState *s)
{ return s && s->valid==AVP_MP_HUDVIEW_MAGIC; }
int avp_mp_hudview_native_capture(AvpMpHudViewState *out)
{
    AvpMpHudViewState s={0};
    if(!out)return 0;
    s.valid=AVP_MP_HUDVIEW_MAGIC;
#define HUDVIEW_SAVE(name) s.name=name;
    HUDVIEW_SAVE(show_coords) HUDVIEW_SAVE(map_on) HUDVIEW_SAVE(show_mt)
    HUDVIEW_SAVE(use_cocoon) HUDVIEW_SAVE(num_cocoons)
    HUDVIEW_SAVE(ccn_xsave) HUDVIEW_SAVE(ccn_ysave)
    memcpy(s.cocoon_data,cocoon_data,sizeof(cocoon_data));
    HUDVIEW_SAVE(old_meter) HUDVIEW_SAVE(scope_frame)
    HUDVIEW_SAVE(pred_meter_previous) HUDVIEW_SAVE(tracker_nearest)
    HUDVIEW_SAVE(pred_meter_sample_a) HUDVIEW_SAVE(pred_meter_sample_b)
    HUDVIEW_SAVE(nrg_x) HUDVIEW_SAVE(nrg_y)
    HUDVIEW_SAVE(nrg_barwidth) HUDVIEW_SAVE(nrg_width)
    HUDVIEW_SAVE(nrg_height) HUDVIEW_SAVE(nrg_nframes)
    HUDVIEW_SAVE(nrg_frame) HUDVIEW_SAVE(nrg_rescale)
    HUDVIEW_SAVE(nrg_size) HUDVIEW_SAVE(hud_bright)
    HUDVIEW_SAVE(nrg_ftime) HUDVIEW_SAVE(nrg_flash)
    HUDVIEW_SAVE(nrg_draw_frame) HUDVIEW_SAVE(nrg_draw_pixels)
    HUDVIEW_SAVE(click_track) HUDVIEW_SAVE(tracker_audio_state)
    HUDVIEW_SAVE(pred_meter_left) HUDVIEW_SAVE(pred_meter_right)
    HUDVIEW_SAVE(old_map) HUDVIEW_SAVE(map_info)
#undef HUDVIEW_SAVE
    *out=s;return 1;
}
int avp_mp_hudview_native_restore(const AvpMpHudViewState *s)
{
    if(!avp_mp_hudview_native_valid(s))return 0;
#define HUDVIEW_LOAD(name) name=s->name;
    HUDVIEW_LOAD(show_coords) HUDVIEW_LOAD(map_on) HUDVIEW_LOAD(show_mt)
    HUDVIEW_LOAD(use_cocoon) HUDVIEW_LOAD(num_cocoons)
    HUDVIEW_LOAD(ccn_xsave) HUDVIEW_LOAD(ccn_ysave)
    memcpy(cocoon_data,s->cocoon_data,sizeof(cocoon_data));
    HUDVIEW_LOAD(old_meter) HUDVIEW_LOAD(scope_frame)
    HUDVIEW_LOAD(pred_meter_previous) HUDVIEW_LOAD(tracker_nearest)
    HUDVIEW_LOAD(pred_meter_sample_a) HUDVIEW_LOAD(pred_meter_sample_b)
    HUDVIEW_LOAD(nrg_x) HUDVIEW_LOAD(nrg_y)
    HUDVIEW_LOAD(nrg_barwidth) HUDVIEW_LOAD(nrg_width)
    HUDVIEW_LOAD(nrg_height) HUDVIEW_LOAD(nrg_nframes)
    HUDVIEW_LOAD(nrg_frame) HUDVIEW_LOAD(nrg_rescale)
    HUDVIEW_LOAD(nrg_size) HUDVIEW_LOAD(hud_bright)
    HUDVIEW_LOAD(nrg_ftime) HUDVIEW_LOAD(nrg_flash)
    HUDVIEW_LOAD(nrg_draw_frame) HUDVIEW_LOAD(nrg_draw_pixels)
    HUDVIEW_LOAD(click_track) HUDVIEW_LOAD(tracker_audio_state)
    HUDVIEW_LOAD(pred_meter_left) HUDVIEW_LOAD(pred_meter_right)
    HUDVIEW_LOAD(old_map) HUDVIEW_LOAD(map_info)
#undef HUDVIEW_LOAD
    return 1;
}

int avp_mp_hudworld_native_valid(const AvpMpHudWorldState *s)
{ return s && s->valid==AVP_MP_HUDWORLD_MAGIC && s->one_sec>0u; }
int avp_mp_hudworld_native_capture(AvpMpHudWorldState *out)
{
    AvpMpHudWorldState s={0};
    if(!out)return 0;
    s.valid=AVP_MP_HUDWORLD_MAGIC;
    s.counttime=counttime;s.counting=counting;
    s.ticktime=ticktime;s.flash_dir=flash_dir;
    s.frames_now=frames_now;s.one_sec=one_sec;
    *out=s;
    return 1;
}
int avp_mp_hudworld_native_restore(const AvpMpHudWorldState *s)
{
    if(!avp_mp_hudworld_native_valid(s))return 0;
    counttime=s->counttime;counting=s->counting;
    ticktime=s->ticktime;flash_dir=s->flash_dir;
    frames_now=s->frames_now;one_sec=s->one_sec;
    return 1;
}
