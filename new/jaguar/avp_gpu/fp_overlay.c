#include "jaguar/avp_gpu/fp_overlay.h"
#include <stddef.h>

/* Exact first-person main-overlay descriptors from the 1994
 * MAZE/MAZESCRN.S human_overlays/alien_overlays/pred_overlays tables.
 * set_opic adds scrn_hwidth/scrn_hheight to these authored offsets; weapon
 * motion and swing are applied later by update_wep.  These are therefore not
 * image-centering anchors and must not be derived from width/height. */
static const AvpFpOverlayDesc fp_overlays[]={
    /* Shotgun: SHOT_X=-195, SHOT_Y=-67. */
    {149,153,176, 94,-150, 28},
    {150,153,208,136,-176,-14},
    {151,153,160,167,-178,-45},
    {152,153,160,169,-178,-47},
    /* Pulse rifle: PULSE_X=-30, PULSE_Y=26. */
    {133,143, 96, 97, -30, 26},
    /* Flamethrower: FLAME_X=-72, FLAME_Y=40 (NEW source block). */
    {154,156, 96, 84, -72, 40},
    /* Smartgun: SMART_X=-40, SMART_Y=23. */
    {135,144,176,103, -40, 23},
    /* Alien tail: FTAIL_X=68, FTAIL_Y=-140. */
    {166,168,128,122,  68,-140},
    {167,168,176,152,  16,-130},
    /* Alien bite: ALBITE_X=0, ALBITE_Y=118. */
    {169,172,160, 91, -80, 27},
    {170,172,160,103, -80, 15},
    {171,172,160, 71, -80, 47},
    /* Alien swipe: ALSWIP_X=54, ALSWIP_Y=-43. */
    {158,164, 96,163,  54,-43},
    {159,164,144,109,  -6, 11},
    {160,164,208, 46,-126, 74},
    /* Predator combi stick: STICK_X=176, STICK_Y=120. */
    {179,184, 80, 48,  98, 72},
    {180,184,144, 78,  48, 42},
    {181,184,160, 99,  24, 21},
    {182,184,176,110,  10, 10},
    /* Predator disk: DISK_X=112, DISK_Y=79. */
    {173,176, 64, 41, 112, 79},
    {174,176,160, 57,  20, 63},
    {175,176,224, 50, -64, 70},
    /* Predator punch: PUNCH_X=80, PUNCH_Y=20. */
    {177,183,112,101,  80, 20},
    {178,183,176,129,   4, -8},
    /* Original MAZESCRN.S::facehug xoverlay: entire 320x240 image.
     * With a 320x228 visible maze raster, y=-6 selects the same visible
     * rows as the original HUG_BUFFER+(6*320) offset. */
    {155,157,320,240,-160,-120}
};

int avp_gpu_fp_overlay_desc(int overlay_id,AvpFpOverlayDesc *out)
{
    if(!out||overlay_id<0||overlay_id>=(int)(sizeof(fp_overlays)/sizeof(fp_overlays[0])))return 0;
    *out=fp_overlays[overlay_id];
    return 1;
}


/* Exact active OT_USE descriptors from MAZE/MAZESCRN.S.  The resource order
 * is corroborated by the preserved FILES.DAT archive layout: 136..142 are the
 * seven 4-bpp use images, while 145..148 are their 16-colour palettes.  The
 * active retail weapon command streams use only flash1/2a/2b/3a/3b; puffa/b
 * are present in the data but their flamethrower commands are commented out. */
static const AvpFpUseOverlayDesc fp_use_overlays[]={
    /* flash1: SHOT_X=-195, SHOT_Y=-67. */
    {136,145, 32,25, -10, 16},
    /* flash2a/b: PULSE_X=-30, PULSE_Y=26. */
    {137,146, 32,24, -10, 17},
    {138,146, 32,30,  -8, 11},
    /* flash3a/b: SMART_X=-40, SMART_Y=23; SMARTF=(-42,18).
     * Retail file 147 is the warm flash CLUT; file 148 is gray puff/smoke. */
    {139,147, 96,80, -50,-10},
    {140,147,112,87, -58,-14}
};

int avp_gpu_fp_use_overlay_desc(int use_id,AvpFpUseOverlayDesc *out)
{
    if(!out||use_id<0||use_id>=(int)(sizeof(fp_use_overlays)/sizeof(fp_use_overlays[0])))return 0;
    *out=fp_use_overlays[use_id];
    return 1;
}

unsigned avp_gpu_fp_vblank_parity(u64 vblank_counter)
{
    return (unsigned)(vblank_counter&1u);
}
