#ifndef AVP_JAGUAR_GPU_HUD_FADE_H
#define AVP_JAGUAR_GPU_HUD_FADE_H

#include "avp_types.h"

/* MAZESCRN.S::InitPXFades/fade_info obtains these pixel8 regions from the
 * original Predator HUD picture. x_fade passes the 0..MAX_FADE intensity
 * increment as the upper word of Jaguar blitter B_IINC. This transformation
 * belongs to the HUD brush; it does not change cloak gameplay state. */
static inline u8 avp_gpu_predator_faded_icon(u8 pixel,int x,int hud_y,
                                            int invis_bright,int medpak_bright)
{
    int level=0;
    if(!pixel)return 0;
    if(x>=202&&x<228&&hud_y>=164&&hud_y<193)level=invis_bright;
    else if(x>=31&&x<66&&hud_y>=75&&hud_y<129)level=medpak_bright;
    if(level<0)level=0;
    if(level>6)level=6;
    return (u8)((unsigned)pixel+(unsigned)level);
}

#endif
