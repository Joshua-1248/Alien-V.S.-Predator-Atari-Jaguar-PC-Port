#include "jaguar/avp_gpu/host_widescreen.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void check(unsigned screen_w,unsigned screen_h,unsigned source_w,
                  double expected_half_tan)
{
    const double f=avp_host_wide_focal(228u);
    const double tan_half=(double)source_w/(2.0*f);
    assert(avp_host_wide_native_width(screen_w,screen_h)==source_w);
    assert(fabs(tan_half-expected_half_tan)<0.005);
    assert(avp_host_has_extra_columns(source_w,228u));
    assert(avp_host_wide_center_width(228u)==304u);
    assert(avp_host_wide_center_width(228u)<source_w);
}
int main(void)
{
    /* 4:3 retains the exact legacy image-plane distance, regardless of the
     * fact that Jaguar's physical 320x228 pixels are not square on a TV. */
    assert(avp_host_wide_native_width(960u,720u)==320u);
    assert(!avp_host_has_extra_columns(320u,228u));
    check(1600u,900u,405u,405.0/304.0);    /* 16:9 */
    check(1600u,1000u,365u,365.0/304.0);   /* 16:10 */
    check(2100u,900u,532u,532.0/304.0);    /* 21:9 */
    check(3200u,900u,811u,811.0/304.0);    /* 32:9 */
    /* Two-player top/bottom on a 16:9 output has twice the horizontal
     * extent of the corresponding single-player viewport. */
    check(1600u,450u,811u,811.0/304.0);
    /* Side-by-side cannot expose extra columns when narrower than 4:3. */
    assert(avp_host_wide_native_width(800u,900u)==320u);
    assert(fabs(avp_host_wide_focal(900u)-600.0)<0.001);
    puts("host widescreen FOV / aspect / centered HUD geometry: PASS");
    return 0;
}
