#ifndef AVP_PC_HYBRID_RETICLE_H
#define AVP_PC_HYBRID_RETICLE_H
#include <stdint.h>
/* Shared pixel footprint and colors of the R193 crosshair. The Vulkan host
 * overlay draws only these non-transparent pixels as a bounding-shaped child;
 * all surrounding pixels remain real GPU scene, with no opaque rectangle. */
static inline int avp_pc_hybrid_reticle_unit(unsigned w,unsigned h)
{
    int unit=(int)((w<h?w:h)/228u);
    if(unit<1)unit=1;
    if(unit>3)unit=3;
    return unit;
}
static inline int avp_pc_hybrid_reticle_pixel(int unit,int dx,int dy,
    uint32_t *rgb)
{
    int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    int stem=(ax<=unit && ay>=2*unit && ay<=5*unit)||
             (ay<=unit && ax>=2*unit && ax<=5*unit);
    int dot=ax<=unit/2 && ay<=unit/2;
    if(!stem && !dot)return 0;
    if(rgb)*rgb=(ax==unit||ay==unit)?0x00101010u:0x00f0f0f0u;
    return 1;
}
#endif
