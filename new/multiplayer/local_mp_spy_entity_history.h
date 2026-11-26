#ifndef AVP_LOCAL_MP_SPY_ENTITY_HISTORY_H
#define AVP_LOCAL_MP_SPY_ENTITY_HISTORY_H

#include "amp.h"
#include "jaguar/avp_gpu/state.h"
#include <string.h>

/* The renderer is owned by the VIEWER, but AMP interpolation history belongs
 * to the WORLD being observed. Save the viewer's 10-Hz AMP history around a
 * spy render, exactly as we already do with its door and weapon history.
 * This is presentation-only. Original AMP simulation and single-player are
 * untouched. */
typedef struct AvpMpSpyEntityHistory {
    s32 x[AVP_NUM_AMPS],y[AVP_NUM_AMPS];
    u16 angle[AVP_NUM_AMPS];
    u32 generation[AVP_NUM_AMPS];
    int have_prev_sim;
} AvpMpSpyEntityHistory;

static inline void avp_mp_spy_entities_begin(AvpAvpGpuState *viewer,
    const AvpAvpGpuState *subject,int subject_history_matches_level,
    const AvpAmp *current,u32 (*generation_at)(unsigned),
    AvpMpSpyEntityHistory *saved)
{
    if(!viewer||!current||!generation_at||!saved)return;
    memcpy(saved->x,viewer->amp_prev_x,sizeof saved->x);
    memcpy(saved->y,viewer->amp_prev_y,sizeof saved->y);
    memcpy(saved->angle,viewer->amp_prev_angle,sizeof saved->angle);
    memcpy(saved->generation,viewer->amp_prev_generation,
           sizeof saved->generation);
    saved->have_prev_sim=viewer->have_prev_sim;
    if(subject && subject_history_matches_level && subject->have_prev_sim){
        memcpy(viewer->amp_prev_x,subject->amp_prev_x,
               sizeof viewer->amp_prev_x);
        memcpy(viewer->amp_prev_y,subject->amp_prev_y,
               sizeof viewer->amp_prev_y);
        memcpy(viewer->amp_prev_angle,subject->amp_prev_angle,
               sizeof viewer->amp_prev_angle);
        memcpy(viewer->amp_prev_generation,subject->amp_prev_generation,
               sizeof viewer->amp_prev_generation);
    }else{
        /* Headless bots have no private renderer snapshot. The CURRENT
         * source-world AMP positions are the only honest reference: never
         * blend them against the observer's unrelated old level. */
        for(unsigned i=0;i<AVP_NUM_AMPS;i++){
            viewer->amp_prev_x[i]=current[i].xpos;
            viewer->amp_prev_y[i]=current[i].ypos;
            viewer->amp_prev_angle[i]=current[i].angle;
            viewer->amp_prev_generation[i]=generation_at(i);
        }
    }
    viewer->have_prev_sim=1;
}

static inline void avp_mp_spy_entities_end(AvpAvpGpuState *viewer,
    const AvpMpSpyEntityHistory *saved)
{
    if(!viewer||!saved)return;
    memcpy(viewer->amp_prev_x,saved->x,sizeof saved->x);
    memcpy(viewer->amp_prev_y,saved->y,sizeof saved->y);
    memcpy(viewer->amp_prev_angle,saved->angle,sizeof saved->angle);
    memcpy(viewer->amp_prev_generation,saved->generation,
           sizeof saved->generation);
    viewer->have_prev_sim=saved->have_prev_sim;
}
#endif
