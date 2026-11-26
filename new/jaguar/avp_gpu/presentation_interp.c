#include "jaguar/avp_gpu/presentation_interp.h"
#include <stdint.h>

/* Retail movement tops out below 0x4300 fixed-point units per 10 Hz tick for
 * the fastest species.  A half-maze-square displacement cannot be ordinary
 * MovePlayer motion, so it is a presentation discontinuity.  Normal angular
 * velocity tops out at 0x0900; 0x2000 likewise leaves ample headroom while
 * catching authored/cardinal facing snaps. */
#define AVP_PRESENT_MAX_CAMERA_STEP 0x00008000L
#define AVP_PRESENT_MAX_ANGLE_STEP  0x2000

static double clamp_alpha(double alpha)
{
    if(alpha<0.0)return 0.0;
    if(alpha>1.0)return 1.0;
    return alpha;
}

static int32_t abs32(int32_t v)
{
    if(v>=0)return v;
    if(v==INT32_MIN)return INT32_MAX;
    return -v;
}

s32 avp_presentation_interpolate_fixed(s32 previous,s32 current,double alpha,s32 max_step)
{
    int32_t delta=(int32_t)((u32)current-(u32)previous);
    alpha=clamp_alpha(alpha);
    if(max_step>=0 && abs32(delta)>max_step)return current;
    return (s32)((double)previous+(double)delta*alpha);
}

s16 avp_presentation_interpolate_small(s16 previous,s16 current,double alpha,s16 max_step)
{
    int delta=(int)current-(int)previous;
    alpha=clamp_alpha(alpha);
    if(max_step>=0 && (delta>max_step || delta<-max_step))return current;
    return (s16)((double)previous+(double)delta*alpha);
}

static int interpolate_pose(const AvpPresentationPose *previous,
                            const AvpPresentationPose *current,
                            double alpha,AvpPresentationPose *out,
                            int continuous)
{
    int32_t dx,dy;
    int16_t da;
    int32_t fractional_delta;
    u16 pa,ca,ia;
    if(!previous||!current||!out)return 0;
    alpha=clamp_alpha(alpha);
    dx=(int32_t)(current->x-previous->x);
    dy=(int32_t)(current->y-previous->y);
    pa=(u16)(previous->angle>>16);
    ca=(u16)(current->angle>>16);
    da=(int16_t)(ca-pa);
    if(abs32(dx)>AVP_PRESENT_MAX_CAMERA_STEP ||
       abs32(dy)>AVP_PRESENT_MAX_CAMERA_STEP ||
       da>AVP_PRESENT_MAX_ANGLE_STEP || da<-AVP_PRESENT_MAX_ANGLE_STEP){
        *out=*current;
        return 0;
    }
    out->x=(u32)((double)previous->x+(double)dx*alpha);
    out->y=(u32)((double)previous->y+(double)dy*alpha);
    if(continuous){
        /* Enhanced heading: interpolate the full 32-bit circular angle,
         * including its fractional word, with the same discontinuity guard. */
        fractional_delta=(int32_t)(current->angle-previous->angle);
        out->angle=previous->angle+(u32)(int32_t)((double)fractional_delta*alpha);
    }else{
        /* Original all-integer path remains bit-for-bit identical. */
        ia=(u16)(pa+(s16)((double)da*alpha));
        out->angle=(current->angle&0x0000ffffu)|((u32)ia<<16);
    }
    return 1;
}

int avp_presentation_interpolate_pose(const AvpPresentationPose *previous,
                                      const AvpPresentationPose *current,
                                      double alpha,AvpPresentationPose *out)
{
    return interpolate_pose(previous,current,alpha,out,0);
}

int avp_presentation_interpolate_pose_continuous(
    const AvpPresentationPose *previous,const AvpPresentationPose *current,
    double alpha,AvpPresentationPose *out)
{
    return interpolate_pose(previous,current,alpha,out,1);
}

/* PT_ALIEN is 4 in the original PLAYER definitions (not a host sprite ID).
 * Compare weapon slots, never the artwork identity, across a 10 Hz boundary. */
#include "player.h"
int avp_presentation_alien_overlay_continuity(s16 prev_species,s16 prev_weapon,
                                              int prev_visible,s16 cur_species,
                                              s16 cur_weapon,int cur_visible)
{
    return prev_visible&&cur_visible&&prev_species==PT_ALIEN&&
           cur_species==PT_ALIEN&&prev_weapon==cur_weapon&&
           cur_weapon>=1&&cur_weapon<=3;
}

u64 avp_presentation_host_frame_period(unsigned hz)
{
    return 1000000000ull/avp_presentation_valid_rate(hz);
}

u64 avp_presentation_next_display_deadline(u64 deadline,u64 now,u64 frame_ns)
{
    if(!frame_ns)return deadline;
    if(deadline>now)return deadline;
    /* Integral future slot instead of now + frame_ns (which loses phase). */
    const u64 count=(now-deadline)/frame_ns+1u;
    if(count>(UINT64_MAX-deadline)/frame_ns)return UINT64_MAX;
    return deadline+count*frame_ns;
}

/* Pure host scheduler arithmetic.  No Jaguar gameplay clock is advanced here.
 * Zero budget means at least one task is ready and should run immediately. */
u64 avp_presentation_sleep_budget(u64 now,u64 next_sim,u64 next_vblank,
                                   u64 next_render,u64 poll_cap_ns)
{
    u64 nearest=next_sim;
    if(next_vblank<nearest)nearest=next_vblank;
    if(next_render<nearest)nearest=next_render;
    if(nearest<=now)return 0;
    if(nearest-now<poll_cap_ns)return nearest-now;
    return poll_cap_ns;
}
