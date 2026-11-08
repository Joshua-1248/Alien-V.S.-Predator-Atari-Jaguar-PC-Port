#ifndef AVP_JAGUAR_AVP_GPU_FP_OVERLAY_H
#define AVP_JAGUAR_AVP_GPU_FP_OVERLAY_H

#include "avp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpFpOverlayDesc {
    int file;
    int palette_file;
    int width,height;
    int x_offset,y_offset; /* authored coordinates relative to screen centre */
} AvpFpOverlayDesc;

typedef struct AvpFpUseOverlayDesc {
    int file;
    int palette_file;
    int width,height;
    int x_offset,y_offset; /* authored coordinates relative to screen centre */
} AvpFpUseOverlayDesc;

int avp_gpu_fp_overlay_desc(int overlay_id,AvpFpOverlayDesc *out);
int avp_gpu_fp_use_overlay_desc(int use_id,AvpFpUseOverlayDesc *out);
/* Retail vf_pulse hides USE2 on even VBLs and USE1 on odd VBLs.  Convert the
 * host's interpolated simulation time to the same 60 Hz presentation parity. */
unsigned avp_gpu_fp_vblank_parity(u64 vblank_counter);

#ifdef __cplusplus
}
#endif
#endif
