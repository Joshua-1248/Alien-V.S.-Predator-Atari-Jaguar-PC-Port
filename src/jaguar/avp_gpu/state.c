#include "jaguar/avp_gpu/state.h"
#include <stdlib.h>
#include <string.h>

void avp_gpu_state_init(AvpAvpGpuState *state)
{
    if (!state) return;
    memset(state,0,sizeof(*state));
    avp_jaguar_pause_op_init(&state->pause_op);
}

void avp_gpu_state_destroy(AvpAvpGpuState *state)
{
    int i,m,r;
    if (!state) return;

    for (i=0;i<74;i++) avp_gpu_free_rgb_image(&state->panel_tex[i]);
    for (i=0;i<18;i++) avp_gpu_free_rgb_image(&state->notice_tex[i]);
    for (i=0;i<3;i++) {
        free(state->hud_base[i]);
        free(state->hud_energy[i]);
        free(state->hud_score_font[i]);
    }
    free(state->hud_weapon_bar);
    free(state->hud_cocoon);
    free(state->hud_native_tiles);
    free(state->tracker_ring);
    free(state->tracker_base1);
    free(state->tracker_base2);
    free(state->tracker_kill);
    free(state->tracker_blips);
    free(state->tracker_basex);
    free(state->qconv);
    for (i=0;i<63;i++) free(state->aaf[i]);
    for (i=0;i<52;i++) free(state->fp_img[i]);
    free(state->computer_bg_img);
    for (i=0;i<29;i++) free(state->computer_overlay_img[i]);
    for (m=0;m<10;m++)
        for (r=0;r<9;r++)
            avp_gpu_free_rgb_image(&state->frontend_img[m][r]);
    free(state->mjp_font);
    free(state->avp_font);
    avp_jaguar_pause_op_destroy(&state->pause_op);
    avp_gpu_maze2_occlusion_destroy(&state->maze_occlusion);

    memset(state,0,sizeof(*state));
}

void avp_gpu_automap_reset(AvpAvpGpuState *state)
{
    if(!state)return;
    memset(state->automap_seen,0,sizeof(state->automap_seen));
    memset(state->automap_bits,0,sizeof(state->automap_bits));
    memset(state->automap_touched,0,sizeof(state->automap_touched));
    state->automap_level=0;
}

void avp_gpu_automap_save(const AvpAvpGpuState *state,AvpGpuAutomapSnapshot *dst)
{
    if(!state||!dst)return;
    memcpy(dst->seen,state->automap_seen,sizeof(dst->seen));
    dst->level=state->automap_level;
    memcpy(dst->bits,state->automap_bits,sizeof(dst->bits));
}
void avp_gpu_automap_load(AvpAvpGpuState *state,const AvpGpuAutomapSnapshot *src)
{
    if(!state||!src)return;
    memcpy(state->automap_seen,src->seen,sizeof(state->automap_seen));
    state->automap_level=src->level;
    memcpy(state->automap_bits,src->bits,sizeof(state->automap_bits));
    memset(state->automap_touched,0,sizeof(state->automap_touched));
}

/* Keep history out of the original game simulation and shared level state.
 * Each archive is owned by exactly one viewer. Store only levels visited. */
void avp_gpu_automap_history_capture(AvpGpuAutomapHistory *history,
                                     const AvpAvpGpuState *state)
{
    AvpGpuAutomapSnapshot *snapshot;
    unsigned level;
    if(!history||!state)return;
    level=state->automap_level;
    if(level<1u||level>AVP_AUTOMAP_HISTORY_LEVELS)return;
    snapshot=history->level[level-1u];
    if(!snapshot){
        snapshot=(AvpGpuAutomapSnapshot *)malloc(sizeof(*snapshot));
        if(!snapshot)return; /* low memory: fall back to ordinary automap */
        history->level[level-1u]=snapshot;
    }
    avp_gpu_automap_save(state,snapshot);
}

void avp_gpu_automap_history_enter(AvpGpuAutomapHistory *history,
                                   AvpAvpGpuState *state,unsigned level,int enabled)
{
    if(!enabled||!history||!state||level<1u||level>AVP_AUTOMAP_HISTORY_LEVELS ||
       state->automap_level==level)return;
    avp_gpu_automap_history_capture(history,state);
    if(history->level[level-1u])
        avp_gpu_automap_load(state,history->level[level-1u]);
    else avp_gpu_automap_reset(state);
    /* The caller makes its normal MAZE5 automap observation afterward,
     * adding only newly revealed strokes. No full map is ever exposed. */
}
void avp_gpu_automap_history_destroy(AvpGpuAutomapHistory *history)
{
    if(!history)return;
    for(unsigned i=0;i<AVP_AUTOMAP_HISTORY_LEVELS;i++){
        free(history->level[i]);
        history->level[i]=NULL;
    }
}
