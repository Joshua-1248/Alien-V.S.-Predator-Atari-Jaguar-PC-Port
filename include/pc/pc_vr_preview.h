#ifndef AVP_PC_VR_PREVIEW_H
#define AVP_PC_VR_PREVIEW_H
#include <stdint.h>
/* Desktop-only stereoscopic preview helpers; no actual headset support. */
void avp_pc_vr_preview_eye(uint32_t x,uint32_t y,uint32_t heading,int continuous,
    float separation,unsigned eye,uint32_t *out_x,uint32_t *out_y);
void avp_pc_vr_preview_blit_eye(const uint32_t *src,unsigned sw,unsigned sh,
    uint32_t *dst,unsigned dw,unsigned dh,unsigned eye);
/* 0: fill each half, 1: fit 4:3, 2: fit square (black bars outside). */
typedef struct AvpPcVrRect {unsigned x,y,w,h;} AvpPcVrRect;
AvpPcVrRect avp_pc_vr_preview_rect(unsigned dw,unsigned dh,unsigned eye,unsigned aspect);
void avp_pc_vr_preview_blit_fit(const uint32_t *src,unsigned sw,unsigned sh,
    uint32_t *dst,unsigned dw,unsigned dh,unsigned eye,unsigned aspect);
/* Off-axis parallel cameras converge only in the IMAGE plane, not by toe-in.
 * Returns additive normalized image-plane tangent for the selected eye. */
double avp_pc_vr_preview_projection_shift(float separation,float convergence,unsigned eye);
uint32_t avp_pc_vr_preview_look_angle(uint32_t game_angle,int degrees);
/* Simple uncompressed PPM (RGB) for stereo inspection, no codecs/libraries. */
int avp_pc_vr_preview_write_ppm(const char *path,const uint32_t *src,
    unsigned width,unsigned height,unsigned x,unsigned y,unsigned w,unsigned h);
#endif
