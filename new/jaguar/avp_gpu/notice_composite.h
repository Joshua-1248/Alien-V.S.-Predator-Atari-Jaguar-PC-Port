#ifndef AVP_GPU_NOTICE_COMPOSITE_H
#define AVP_GPU_NOTICE_COMPOSITE_H
#include "jaguar/avp_gpu/bridge.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Exact LEVELS.S GetPanels source-centred overlay placement on FILES 16.
 * Both RGB and CRY samples are copied; shading remains a later GPU step. */
int avp_gpu_notice_composite(const AvpGpuRgbImage *base,
                             const AvpGpuRgbImage *lettering,
                             const AvpGpuRgbImage *number,
                             int yoffset,AvpGpuRgbImage *out);
#ifdef __cplusplus
}
#endif
#endif
