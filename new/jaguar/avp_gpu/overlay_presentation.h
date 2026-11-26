#ifndef AVP_JAGUAR_AVP_GPU_OVERLAY_PRESENTATION_H
#define AVP_JAGUAR_AVP_GPU_OVERLAY_PRESENTATION_H

#include "jaguar/avp_gpu/state.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include "mazescrn.h"

/* R195: the host crosshair and the first-person weapon must see the exact
 * SAME interpolated presentation pose. This is read-only, host-side state:
 * neither the Jaguar firing direction nor its 10-Hz weapon simulation changes.
 * Keep the original continuity guards and 32-unit swing-discontinuity limit. */
static inline void avp_gpu_overlay_present_state(const AvpAvpGpuState *state,
    AvpFirstPersonOverlayState *st,int smooth_presentation,
    s16 species,s16 weapon)
{
    if(!state||!st||!smooth_presentation||!state->prev_overlay_snapshot_valid||
       !st->overlay_visible||!state->prev_overlay_visible||st->intro_preview)return;
    const int same_alien_weapon=avp_presentation_alien_overlay_continuity(
        state->prev_overlay_species,state->prev_overlay_weapon,
        state->prev_overlay_visible,species,weapon,st->overlay_visible);
    const int same_sprite=state->prev_overlay_id==st->overlay_id;
    if(same_alien_weapon||same_sprite){
        const s32 max_offset=same_alien_weapon?-1:(32L<<16);
        const double a=state->render_alpha;
        st->x=avp_presentation_interpolate_fixed(state->prev_wep_x,st->x,a,max_offset);
        st->y=avp_presentation_interpolate_fixed(state->prev_wep_y,st->y,a,max_offset);
        st->swing_x=avp_presentation_interpolate_small(state->prev_swing_x,st->swing_x,a,32);
        st->swing_y=avp_presentation_interpolate_small(state->prev_swing_y,st->swing_y,a,32);
    }
}

/* R196: the crosshair is a host UI element, not a first-person OVER
 * sprite. Sway smoothing must not depend on overlay visibility or sprite ID:
 * an equipped weapon may temporarily have no artwork on screen. Preserve
 * the existing visible weapon's pose interpolation for its own art and use
 * the SAME small-step interpolation for reticle sway, guarded by actual
 * species/weapon continuity and the original 32-unit discontinuity bound.
 * Presentation-only; the 10-Hz game state is not modified. */
static inline void avp_gpu_crosshair_present_state(const AvpAvpGpuState *state,
    AvpFirstPersonOverlayState *st,int smooth_presentation,
    s16 species,s16 weapon)
{
    s16 raw_swing_x,raw_swing_y;
    if(!state||!st)return;
    raw_swing_x=st->swing_x;raw_swing_y=st->swing_y;
    avp_gpu_overlay_present_state(state,st,smooth_presentation,species,weapon);
    if(!smooth_presentation||!state->prev_overlay_snapshot_valid||
       st->intro_preview||state->prev_overlay_species!=species||
       state->prev_overlay_weapon!=weapon){
        st->swing_x=raw_swing_x;st->swing_y=raw_swing_y;
        return;
    }
    st->swing_x=avp_presentation_interpolate_small(
        state->prev_swing_x,raw_swing_x,state->render_alpha,32);
    st->swing_y=avp_presentation_interpolate_small(
        state->prev_swing_y,raw_swing_y,state->render_alpha,32);
}

#endif
