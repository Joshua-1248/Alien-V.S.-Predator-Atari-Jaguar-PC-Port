#ifndef AVP_PC_HYBRID_VIEWPORT_H
#define AVP_PC_HYBRID_VIEWPORT_H
/* R361: game-owned Vulkan viewport exactly mirrors the original X11
 * letterbox and nearest32 source pixel mapping. No Vulkan SDK dependency. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AvpHybridViewport {
    uint32_t x,y,width,height;
    uint32_t source_width,source_height;
} AvpHybridViewport;
int avp_hybrid_viewport_make(uint32_t surface_w,uint32_t surface_h,
    uint32_t present_w,uint32_t present_h,
    uint32_t source_w,uint32_t source_h,AvpHybridViewport *out);
int avp_hybrid_viewport_source_pixel(const AvpHybridViewport *v,
    uint32_t screen_x,uint32_t screen_y,uint32_t *source_x,
    uint32_t *source_y);
#ifdef __cplusplus
}
#endif
#endif
