#ifndef AVP_GPU_AUTOMAP_ENEMY_MARKER_H
#define AVP_GPU_AUTOMAP_ENEMY_MARKER_H
/* RE41 R227: host-only cheat. Real AMP positions are signed 16.16 world
 * coordinates; never truncate to map-cell coordinates for the overlay. */
#include "amp.h"
#include <math.h>

static inline char avp_gpu_automap_enemy_letter(const AvpAmp *a)
{
    if(!a || (!a->mode && !a->host_static) ||
       (a->flags & ((1u<<AMP_DEAD)|(1u<<AMP_PLAYER))) ||
       a->animseq==AS_DEATH)return 0;
    /* AMP.S make_dead_egg keeps a *visible static* AC_EGG corpse: its
     * animation remains AS_STAND, so generic death-animation filtering
     * alone leaves a false 'E' on the cheat map. DEF_DEADEGG (56) marks
     * the actual destroyed template; zero energy also covers the instant
     * after a lethal hit but before the next AMP state update. Do NOT
     * delete the corpse, mutate AMP data or affect living eggs. */
    if(a->creature==AC_EGG && (a->astype==56 || a->energy<=0))return 0;
    switch(a->creature){
    case AC_ALIEN:case AC_ACRAWL:return 'A';
    case AC_AQUEEN:return 'Q';
    case AC_FACEHUG:return 'F';
    case AC_EGG:return 'E';
    case AC_HUMAN:return 'M';
    case AC_PREDATOR:return 'P';
    default:return 0; /* projectiles, effects, items, carcasses are not enemies */
    }
}

/* Inverse of MAZE5.GAS's 16.16 raster sample transform; this matches the
 * existing same-level multiplayer map-dot projection. Input positions keep
 * ALL 16 fractional bits (including negative and subcell offsets).
 * Screen coordinates are rounded only for the final pixel. */
static inline int avp_gpu_automap_enemy_screen(s32 camera_x,s32 camera_y,
    s32 camera_sin,s32 camera_cos,double zoom,
    s32 enemy_x,s32 enemy_y,int center_x,int center_y,int *out_x,int *out_y)
{
    const double sn=(double)camera_sin, cs=(double)camera_cos;
    const double denom=2.0*(sn*sn+cs*cs);
    double dx,dy,sx,sy;
    if(!out_x||!out_y || denom<=1.0 || !isfinite(zoom) || zoom<=0.0)return 0;
    dx=((double)enemy_x-(double)camera_x)*8.0;
    dy=((double)enemy_y-(double)camera_y)*8.0;
    sx=(double)center_x+((sn*dx+cs*dy)/denom)*zoom;
    sy=(double)center_y+((-cs*dx+sn*dy)/denom)*zoom;
    if(!isfinite(sx)||!isfinite(sy)||sx<-1000000.0||sx>1000000.0||
       sy<-1000000.0||sy>1000000.0)return 0;
    *out_x=(int)lrint(sx);*out_y=(int)lrint(sy);
    return 1;
}
#endif
