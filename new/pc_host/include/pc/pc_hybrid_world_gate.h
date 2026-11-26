#ifndef AVP_PC_HYBRID_WORLD_GATE_H
#define AVP_PC_HYBRID_WORLD_GATE_H
/* R361: compare an independently reconstructed source-texture world against
 * the Software world's pre-actor RGB. Only exactly matching frames may replace
 * the Software world with GPU samples. This gate never changes game pixels. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_HYBRID_WORLD_GATE_MAX_PIXELS (4096u*4096u)
/* Expected is tightly packed RGBA bytes in TOP-to-BOTTOM row order. Software
 * world is native XRGB (0x00RRGGBB) with a specified pixel stride. Alpha must
 * be 255 even for a black/uncovered world pixel. Always fail closed; zero
 * mismatches is the ONLY successful result. */
int avp_hybrid_world_gate_compare(const uint8_t *expected_rgba,
    size_t expected_bytes,const uint32_t *software_world,uint32_t stride,
    uint32_t width,uint32_t height,uint32_t *mismatches);
#ifdef __cplusplus
}
#endif
#endif
