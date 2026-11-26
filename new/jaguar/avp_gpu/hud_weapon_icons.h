#ifndef AVP_JAGUAR_GPU_HUD_WEAPON_ICONS_H
#define AVP_JAGUAR_GPU_HUD_WEAPON_ICONS_H

#include "avp_types.h"

/* Retail AVP.jag / RDB.DTA weapon records at $23676...$23DD8.
 * Each record starts with four big-endian words specifying the icon's
 * HUD-local X,Y,width,height.  This table is *not* an artist's approximation.
 * Source lookup: $235DA -> three weapon pointer tables; RDB.TEXT::
 * $E978 builds the Blitter rectangle descriptor from these same records. */
typedef struct AvpHudWeaponIconRect {
    s16 x,y,width,height;
} AvpHudWeaponIconRect;

static inline AvpHudWeaponIconRect avp_hud_weapon_rect(unsigned species,unsigned weapon)
{
    static const AvpHudWeaponIconRect rects[3][6]={
        {{246,48,50,18},{249,88,44,18},{247,127,48,31},{245,176,51,19},{0,0,0,0},{0,0,0,0}},
        {{246,145,46,34},{255,39,28,44},{244,97,47,31},{0,0,0,0},{0,0,0,0},{0,0,0,0}},
        {{255,59,34,8},{249,87,46,27},{259,129,25,25},{251,175,40,18},{202,164,26,29},{31,75,35,54}}
    };
    const AvpHudWeaponIconRect none={0,0,0,0};
    if(species>2u||weapon<1u||weapon>6u)return none;
    return rects[species][weapon-1u];
}

/* RDB.TEXT $EB52..$EBF8 executes two B_CMD=$00000200 operations, with
 * A1_PIXEL at (x,y) then (x+1,y+1), XADDINC=2 and A1_STEP Y=2.
 * B_PATD was cleared to zero by $EAC4.  Thus not-owned icons have the
 * two parity-matched sets of pixels cleared.  The other pixels retain
 * the original ROM bitmap, not a host-authored blend/transparency.
 *
 * RDB.TEXT $EAC4 uses B_IINC=brightness<<16, SRCEN|SRCSHADE and
 * DCOMPEN with zero-pattern inhibition to re-copy the selected icon
 * with the source palette intensity offset.  Brightness is driven by
 * MAZESCRN.S fade_selects at the *game tick*, not the host refresh.
 *
 * The Predator's cloak and medkit brightness has an independent PX
 * Blitter pass, already preserved by avp_gpu_predator_faded_icon().
 * Do not double-apply the selection increment to those two icons. */
static inline u8 avp_gpu_weapon_icon_pixel(u8 pixel,unsigned species,int x,int hud_y,
                                            u8 owned_mask,int in_weapon,int in_bright,
                                            int out_weapon,int out_bright)
{
    unsigned max_weapons=species==0u?4u:species==1u?3u:species==2u?6u:0u;
    if(!pixel||!max_weapons)return pixel;
    for(unsigned slot=1;slot<=max_weapons;slot++){
        AvpHudWeaponIconRect r=avp_hud_weapon_rect(species,slot);
        int dx=x-r.x,dy=hud_y-r.y;
        /* $EB52 computes B_COUNT from (width+1,height+1)>>1 for
         * both XADDINC=2 checkerboard passes.  Odd-sized source rectangles
         * can therefore clear one pixel outside their right/bottom edge;
         * preserve that authored Blitter traversal instead of clipping the
         * result to an invented icon-only rectangle. */
        if(!(owned_mask&(1u<<slot))){
            int half_w=(r.width+1)/2,half_h=(r.height+1)/2;
            if((dx>=0&&dy>=0&&!(dx&1)&&!(dy&1)&&
                dx/2<half_w&&dy/2<half_h) ||
               (dx>=1&&dy>=1&&(dx&1)&&(dy&1)&&
                dx/2<half_w&&dy/2<half_h))return 0u;
        }
        if(dx<0||dy<0||dx>=r.width||dy>=r.height)continue;
        if(species==2u&&slot>=5u)return pixel;
        {
            int level=slot==(unsigned)in_weapon?in_bright:
                      slot==(unsigned)out_weapon?out_bright:0;
            if(level<0)level=0;
            if(level>6)level=6;
            return (u8)((unsigned)pixel+(unsigned)level);
        }
    }
    return pixel;
}
#endif
