#ifndef AVP_PC_CROSSHAIR_H
#define AVP_PC_CROSSHAIR_H
#include <stdint.h>
#include <stddef.h>
#include "pc/pc_hybrid_reticle.h"
/* Host-presentation crosshair. Weapon sway is the existing Jaguar swing_x
 * (native 320-pixel units), never the firing/aim angle. Only the supplied
 * viewport is touched, so split-screen neighboring players are isolated. */
static inline int avp_pc_crosshair_x(unsigned width,int sway_enabled,int swing_x)
{
    int dx;
    if(!sway_enabled || !width)return (int)(width/2u);
    if(swing_x>24)swing_x=24;
    if(swing_x< -24)swing_x=-24;
    dx=(int)((int64_t)swing_x*(int64_t)width/320);
    return (int)(width/2u)+dx;
}
static inline void avp_pc_crosshair_draw(uint32_t *pixels,unsigned pitch,
     unsigned canvas_w,unsigned canvas_h,unsigned x,unsigned y,unsigned w,unsigned h,
     int sway_enabled,int swing_x)
{
    if(!pixels||!w||!h||x>=canvas_w||y>=canvas_h||
       w>canvas_w-x||h>canvas_h-y||pitch<canvas_w)return;
    int cx=avp_pc_crosshair_x(w,sway_enabled,swing_x)+(int)x;
    int cy=(int)y+(int)(h/2u);
    /* A subtle outlined + reticle with a clear one-pixel aim center.
     * Its displacement follows the weapon's native swing, not bob or look. */
    int unit=avp_pc_hybrid_reticle_unit(w,h);
    for(int dy=-5*unit;dy<=5*unit;dy++)for(int dx=-5*unit;dx<=5*unit;dx++){
        uint32_t rgb;
        int px=cx+dx,py=cy+dy;
        if(!avp_pc_hybrid_reticle_pixel(unit,dx,dy,&rgb))continue;
        if(px<(int)x||py<(int)y||px>=(int)(x+w)||py>=(int)(y+h))continue;
        pixels[(size_t)py*pitch+(unsigned)px]=rgb;
    }
}
#endif
