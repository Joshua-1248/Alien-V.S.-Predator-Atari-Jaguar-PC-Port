#ifndef AVP_JAGUAR_AVP_GPU_PRESENTATION_INTERP_H
#define AVP_JAGUAR_AVP_GPU_PRESENTATION_INTERP_H

#include "avp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpPresentationPose {
    u32 x;
    u32 y;
    u32 angle;
} AvpPresentationPose;

/* Smooth Presentation is presentation-only.  Refuse to interpolate across a
 * state discontinuity (teleport/load/forced facing change), because blending
 * those states creates visible motion the 10 Hz game simulation never made.
 * Return 1 when interpolation was used, 0 when the current authoritative pose
 * was snapped to directly. */
int avp_presentation_interpolate_pose(const AvpPresentationPose *previous,
                                      const AvpPresentationPose *current,
                                      double alpha,
                                      AvpPresentationPose *out);

/* PC Enhanced Analog Controls only: interpolate every fractional heading bit.
 * Legacy/Jaguar pose interpolation remains unchanged. */
int avp_presentation_interpolate_pose_continuous(
    const AvpPresentationPose *previous,const AvpPresentationPose *current,
    double alpha,AvpPresentationPose *out);

/* Same rule for fixed-point overlay coordinates.  This is used for the first-
 * person object list: ordinary per-tick weapon motion is smoothed, while a
 * script/reset jump is presented immediately instead of sliding through it. */
s32 avp_presentation_interpolate_fixed(s32 previous,s32 current,double alpha,s32 max_step);
s16 avp_presentation_interpolate_small(s16 previous,s16 current,double alpha,s16 max_step);

/* Host-only 10 Hz overlay continuity: original Alien attack sprite ID can
 * change while the same selected weapon stays visible.  Slot zero, other
 * species and acquisition/disappearance do not qualify. */
int avp_presentation_alien_overlay_continuity(s16 prev_species,s16 prev_weapon,
                                              int prev_visible,s16 cur_species,
                                              s16 cur_weapon,int cur_visible);
/* Host presentation-rate table, deliberately separate from physical display
 * mode selection and the retail Jaguar 60 Hz VBL clock. One definition is
 * shared by settings validation, F12 navigation, and the host scheduler. */
static inline unsigned avp_presentation_rate_count(void){return 21u;}
static inline unsigned avp_presentation_rate_at(unsigned index)
{
    static const unsigned rates[21]={
        24u,30u,50u,60u,75u,85u,90u,100u,120u,144u,165u,
        180u,200u,240u,280u,300u,360u,480u,500u,540u,1000u
    };
    return index<21u?rates[index]:60u;
}
static inline unsigned avp_presentation_valid_rate(unsigned hz)
{
    for(unsigned i=0;i<avp_presentation_rate_count();i++)
        if(avp_presentation_rate_at(i)==hz)return hz;
    return 60u;
}
/* One adjacent menu step, including wrap at the endpoints. */
static inline unsigned avp_presentation_rate_neighbor(unsigned hz,int direction)
{
    const unsigned count=avp_presentation_rate_count();
    unsigned index=3u; /* Unsupported values reset to the 60 Hz slot. */
    for(unsigned i=0;i<count;i++)
        if(avp_presentation_rate_at(i)==hz){index=i;break;}
    if(direction==0)return avp_presentation_rate_at(index);
    return avp_presentation_rate_at(direction>0?(index+1u)%count:
                                     (index+count-1u)%count);
}
/* Optional PC-only output frequency. The retail 60 Hz VBL stays separate. */
u64 avp_presentation_host_frame_period(unsigned hz);
/* Rephase a late host rendering deadline onto the existing host frame grid;
 * never alter the authoritative 10 Hz gameplay deadline. */
u64 avp_presentation_next_display_deadline(u64 deadline,u64 now,u64 frame_ns);
/* Host event-poll pacing: never voluntarily sleep beyond the nearest
 * authoritative simulation, Jaguar-VBL or display deadline.  poll_cap_ns
 * retains the existing maximum input/event service interval. */
u64 avp_presentation_sleep_budget(u64 now,u64 next_sim,u64 next_vblank,
                                   u64 next_render,u64 poll_cap_ns);

#ifdef __cplusplus
}
#endif
#endif
