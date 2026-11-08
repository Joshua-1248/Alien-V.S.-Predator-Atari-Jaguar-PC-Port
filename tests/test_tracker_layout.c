/* The Jaguar tracker brushes are composed in HUD-local, not screen, space.
 * This regression guards the R106 double application of the +14 OP offset. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned at(const AvpTrackerCanvas *c,int x,int y)
{
    return c->pixel[(size_t)(y-TRACKER_TILE_Y)*TRACKER_TILE_W +
                    (unsigned)(x-TRACKER_TILE_X)];
}
int main(void)
{
    AvpAvpGpuState state={0};
    AvpTrackerCanvas canvas={0};
    unsigned char upper[96u*64u]={0},lower[96u*25u]={0},extra[16u*2u]={0};
    unsigned char digits[90]={0},tiny[80]={0};
    upper[0]=1; upper[96u*63u]=2; /* top left and last row */
    lower[0]=3; lower[96u*24u]=4; /* abutting row and last row */
    extra[0]=5;
    state.tracker_base1=upper;state.tracker_base1_size=sizeof(upper);
    state.tracker_base2=lower;state.tracker_base2_size=sizeof(lower);
    state.tracker_basex=extra;state.tracker_basex_size=sizeof(extra);
    tracker_restore_upper(&state,&canvas);
    tracker_restore_lower(&state,&canvas);
    assert(at(&canvas,16,111)==1u);
    assert(at(&canvas,16,174)==2u);
    assert(at(&canvas,16,175)==3u);
    assert(at(&canvas,16,199)==4u);
    assert(at(&canvas,112,198)==5u);
    assert(at(&canvas,16,125)==0u); /* misplaced in R106 */
    /* Glyphs occupy the original unshifted local meter positions. */
    digits[0]=0xff;tiny[0]=0xe0;
    state.hud_score_font[0]=digits;state.hud_score_font_size[0]=sizeof(digits);
    state.hud_tiny_font=tiny;state.hud_tiny_font_size=sizeof(tiny);
    tracker_draw_whole(&state,&canvas,0,48,184);
    tracker_draw_hundredths(&state,&canvas,0,70,188);
    assert(at(&canvas,48,184)==0xe3u);
    assert(at(&canvas,48,185)==0xebu);
    assert(at(&canvas,70,188)==0xe3u);
    assert(at(&canvas,71,188)==0xe3u);
    assert(at(&canvas,73,188)==0xebu);
    /* The entire separately blitted scanner replaces the original HUD
     * indices *before* the one and only RMW screen composite. R109 excluded
     * two divider pixels only, so the HUD rim and floor border were still
     * blended again under the scan brush: the reported doubled opacity. */
    assert(!tracker_replaces_main_hud_pixel(0u,1,20,124));
    assert(tracker_replaces_main_hud_pixel(0u,1,20,125)); /* left HUD rail */
    assert(tracker_replaces_main_hud_pixel(0u,1,20,190));
    assert(tracker_replaces_main_hud_pixel(0u,1,78,125)); /* old divider */
    assert(tracker_replaces_main_hud_pixel(0u,1,79,211));
    assert(tracker_replaces_main_hud_pixel(0u,1,80,145)); /* full footprint */
    assert(tracker_replaces_main_hud_pixel(0u,1,30,212)); /* lower edge */
    assert(tracker_replaces_main_hud_pixel(0u,1,30,213));
    assert(tracker_replaces_main_hud_pixel(0u,1,112,212)); /* C4 strip */
    assert(tracker_replaces_main_hud_pixel(0u,1,127,213));
    assert(!tracker_replaces_main_hud_pixel(0u,1,128,213));
    assert(!tracker_replaces_main_hud_pixel(0u,1,112,211));
    assert(!tracker_replaces_main_hud_pixel(0u,1,112,145));
    assert(!tracker_replaces_main_hud_pixel(0u,1,78,214));
    assert(!tracker_replaces_main_hud_pixel(0u,1,15,150));
    assert(!tracker_replaces_main_hud_pixel(0u,0,20,190));
    assert(!tracker_replaces_main_hud_pixel(0u,0,78,190));
    assert(!tracker_replaces_main_hud_pixel(1u,1,20,190));
    assert(!tracker_replaces_main_hud_pixel(2u,1,78,190));
    puts("Jaguar motion tracker full replacement of indexed HUD brush footprint PASS");
    return 0;
}
