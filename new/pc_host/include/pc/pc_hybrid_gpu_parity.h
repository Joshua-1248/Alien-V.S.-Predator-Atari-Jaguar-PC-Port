#ifndef AVP_PC_HYBRID_GPU_PARITY_H
#define AVP_PC_HYBRID_GPU_PARITY_H
/* R365: verify a REAL Vulkan-composited frame against the same frame's
 * original complete Software image BEFORE the first Vulkan present.
 * SDK-independent implementation; no Software RGB pixels reach GPU upload. */
#include "pc/pc_hybrid_overlay.h"
#include "pc/pc_hybrid_viewport.h"
#include <stdint.h>
#include "pc_scanlines.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* R381: A byte-exact CPU source snapshot and the per-frame Software world
 * oracle remain mandatory for changed texels. The first actual GPU frame
 * and every rebuilt pipeline still need full GPU pixel proof. An unchanged,
 * previously proven shader pipeline need not synchronously re-read the
 * entire swapchain merely because the same upload path got new texels.
 * Developers may force the original global proof for every changed image. */
int avp_hybrid_gpu_reproof_for_texture_change(int gpu_already_verified,
    int texture_changed,int strict_texture_verification);
int avp_hybrid_gpu_parity_compare(const uint8_t *gpu,size_t gpu_bytes,
    uint32_t width,uint32_t height,uint32_t format,
    const AvpHybridViewport *viewport,const AvpHybridOverlay *overlay,
    uint32_t *mismatches);
int avp_hybrid_gpu_parity_compare_scanlines(const uint8_t *gpu,size_t gpu_bytes,
    uint32_t width,uint32_t height,uint32_t format,
    const AvpHybridViewport *viewport,const AvpHybridOverlay *overlay,
    const AvpPcScanlines *scanlines,uint32_t *mismatches);
#ifdef __cplusplus
}
#endif
#endif
