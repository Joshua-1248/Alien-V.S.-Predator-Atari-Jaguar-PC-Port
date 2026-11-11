/* R89: MJP CPU events never create extra X11 pictures between original
 * video-field callbacks during the story/attract sequence. */
#include "pc/pc_attract_fields.h"
#include "pc/pc_title_reference.h"
#include "pc/pc_frontend_field_clock.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const unsigned intro_events[]={
        0x4d4a5041u, /* MJPA: original typing sound */
        0x4d4a5054u, /* MJPT: text resource */
        0x4d4a5050u, /* MJPP: start CROSS */
        0x4d4a5058u, /* MJPX: x/phase, not a VBlank */
        0x4d4a5053u, /* MJPS: GPU completion */
        0x4d4a5043u, /* MJPC: live text cursor advances */
        0x4d4a505au, /* MJPZ: cursor removed at text completion */
        0x434c5231u, /* CLR1: blitter work */
        0x5449434bu  /* TICK: already presented from wait_vblank */
    };
    for(unsigned m=0;m<2u;m++){
        const unsigned mode=m?AVP_MJP_ESCAPE:AVP_MJP_INTRO;
        for(unsigned i=0;i<sizeof(intro_events)/sizeof(intro_events[0]);i++)
            assert(!avp_pc_frontend_present_on_event(mode,intro_events[i]));
    }
    /* No disturbance to the user-confirmed original Jaguar title path. */
    assert(avp_pc_frontend_present_on_event(AVP_MJP_TITLE,0x4d4a5058u));
    assert(!avp_pc_frontend_present_on_event(AVP_MJP_TITLE,0x5449434bu));
    assert(avp_pc_frontend_present_on_event(AVP_MJP_SELECT,0x534f424au));
    assert(avp_pc_frontend_present_on_event(AVP_MJP_FAME,0x4d4a5041u));
    /* Nominal 60-Hz deadlines correct for render time instead of adding it. */
    assert(avp_pc_title_next_deadline(0u,100u)==16666767ull);
    assert(avp_pc_title_next_deadline(16666767ull,9000000ull)==33333434ull);
    assert(avp_pc_title_next_deadline(16666767ull,2000000000ull)==2016666667ull);
    /* Host redraw time must not extend original Jaguar's 800 field count.
     * PAL follows the cartridge's 50 fields per second; logo uses its
     * separately verified native timeline. */
    assert(avp_pc_mjp_field_paced(AVP_MJP_TITLE));
    assert(avp_pc_mjp_field_paced(AVP_MJP_INTRO));
    assert(avp_pc_mjp_field_paced(AVP_MJP_ESCAPE));
    assert(avp_pc_mjp_field_paced(AVP_MJP_SIM_TERMINATED));
    assert(avp_pc_mjp_field_paced(AVP_MJP_BASE_EXPLODES));
    assert(!avp_pc_mjp_field_paced(AVP_MJP_SELECT));
    assert(!avp_pc_mjp_field_paced(AVP_MJP_FAME));
    assert(avp_pc_mjp_next_deadline(0u,100u,60u)==16666767ull);
    assert(avp_pc_mjp_next_deadline(16666767ull,9000000ull,60u)==33333434ull);
    assert(avp_pc_mjp_next_deadline(0u,100u,50u)==20000100ull);
    assert(avp_pc_mjp_next_deadline(20000100ull,30000000ull,50u)==40000100ull);
    puts("PASS: intro/escape field-only rendering, title untouched, menu/ending frame deadlines");
    return 0;
}
