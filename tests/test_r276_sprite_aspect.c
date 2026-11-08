#include "jaguar/avp_gpu/sprite_projection.h"
#include "jaguar/avp_gpu/host_widescreen.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void near(double lhs,double rhs) { assert(fabs(lhs-rhs)<1e-10); }
int main(void)
{
    double x,y,x2,y2;
    const int r=108;
    /* Preserve 4:3 calculation, including the original 320x228 mode. */
    avp_gpu_sprite_viewport_scale(320u,228u,160.0,1.0,r,&x,&y);
    near(x,0.84375);near(x,y);
    avp_gpu_sprite_viewport_scale(640u,456u,320.0,1.0,r,&x2,&y2);
    near(x2,2*x);near(y2,2*y);

    /* Widening a view adds world columns, not width to a sprite. */
    avp_gpu_sprite_viewport_scale(405u,228u,avp_host_wide_focal(228u),1.0,r,&x,&y);
    near(x,0.84375*(152.0/160.0));near(y,0.84375);
    avp_gpu_sprite_viewport_scale(532u,228u,avp_host_wide_focal(228u),1.0,r,&x2,&y2);
    near(x2,x);near(y2,y); /* 21:9 vs 16:9 at the same picture-plane scale */
    avp_gpu_sprite_viewport_scale(811u,228u,avp_host_wide_focal(228u),1.0,r,&x2,&y2);
    near(x2,x);near(y2,y); /* also 32:9 */

    /* High-res 16:9 retains the SAME rendered object dimensions as the
     * low-res 16:9 after uniform scaling to the target presentation size. */
    avp_gpu_sprite_viewport_scale(1920u,1080u,avp_host_wide_focal(1080u),1.0,r,&x2,&y2);
    near(x2,x*(1080.0/228.0));near(y2,y*(1080.0/228.0));
    /* Distance still halves projected size on both axes. */
    avp_gpu_sprite_viewport_scale(1920u,1080u,avp_host_wide_focal(1080u),2.0,r,&x,&y);
    near(x,x2/2.0);near(y,y2/2.0);
    puts("AAF widescreen sprite/wall projection consistency: PASS");
    return 0;
}
