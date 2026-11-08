#ifndef AVP_JAGUAR_AVP_GPU_FRONTEND_RENDERER_H
#define AVP_JAGUAR_AVP_GPU_FRONTEND_RENDERER_H

#include "avp_types.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void avp_gpu_frontend_enter_mode(AvpAvpGpuState *state,unsigned mode);
void avp_gpu_frontend_event(AvpAvpGpuState *state,unsigned event,s32 arg0,s32 arg1);
void avp_gpu_frontend_draw_text(AvpAvpGpuState *state,unsigned line,const char *text,unsigned x);
void avp_gpu_frontend_select_instructions(AvpAvpGpuState *state,const char *rotate,const char *fire);
void avp_gpu_frontend_select_briefing(AvpAvpGpuState *state,const char *lines,const char *status);
void avp_gpu_frontend_select_prompt(AvpAvpGpuState *state,const char *prompt);
int avp_gpu_frontend_select_loading_ready(const AvpAvpGpuState *state);
void avp_gpu_frontend_present(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,unsigned mode,s32 choice);
int avp_gpu_frontend_transition_done(AvpAvpGpuState *state,unsigned phase);

#ifdef __cplusplus
}
#endif

#endif
