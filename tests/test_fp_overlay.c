#include "jaguar/avp_gpu/fp_overlay.h"
#include <assert.h>

static void chk(int id,int file,int pal,int w,int h,int x,int y)
{
    AvpFpOverlayDesc d;assert(avp_gpu_fp_overlay_desc(id,&d));
    assert(d.file==file&&d.palette_file==pal&&d.width==w&&d.height==h);
    assert(d.x_offset==x&&d.y_offset==y);
}
static void chku(int id,int file,int pal,int w,int h,int x,int y)
{
    AvpFpUseOverlayDesc d;assert(avp_gpu_fp_use_overlay_desc(id,&d));
    assert(d.file==file&&d.palette_file==pal&&d.width==w&&d.height==h);
    assert(d.x_offset==x&&d.y_offset==y);
}
int main(void)
{
    /* Representative main entries from each source table plus edge/offscreen cases. */
    chk(0,149,153,176,94,-150,28);       /* gshot1 */
    chk(3,152,153,160,169,-178,-47);     /* gshot4 */
    chk(4,133,143,96,97,-30,26);         /* pulsea */
    chk(7,166,168,128,122,68,-140);      /* ftail1: deliberately starts above raster */
    chk(12,158,164,96,163,54,-43);       /* alswip1 */
    chk(18,182,184,176,110,10,10);       /* combi4 */
    chk(21,175,176,224,50,-64,70);       /* disk3 */
    chk(23,178,183,176,129,4,-8);        /* punch2 */
    {AvpFpOverlayDesc d;assert(!avp_gpu_fp_overlay_desc(-1,&d));assert(!avp_gpu_fp_overlay_desc(25,&d));}

    /* Active retail OT_USE objects from MAZESCRN.S. */
    chku(0,136,145,32,25,-10,16);        /* shotgun flash1 */
    chku(1,137,146,32,24,-10,17);        /* pulse flash2a */
    chku(2,138,146,32,30,-8,11);         /* pulse flash2b */
    chku(3,139,147,96,80,-50,-10);       /* smartgun flash3a */
    chku(4,140,147,112,87,-58,-14);      /* smartgun flash3b */
    {AvpFpUseOverlayDesc d;assert(!avp_gpu_fp_use_overlay_desc(-1,&d));assert(!avp_gpu_fp_use_overlay_desc(5,&d));}

    /* vf_pulse: even VBL shows USE1; odd VBL shows USE2. */
    assert(avp_gpu_fp_vblank_parity(0)==0u);
    assert(avp_gpu_fp_vblank_parity(1)==1u);
    assert(avp_gpu_fp_vblank_parity(2)==0u);
    assert(avp_gpu_fp_vblank_parity(101)==1u);
    return 0;
}
