/* Native X11 integration smoke test; run with xvfb-run or a desktop DISPLAY.
 * The test verifies repeated focus transitions and cleanup do not crash;
 * an actual desktop session daemon is needed to prove idle-lock suppression.
 */
#include "pc_idle_inhibit.h"
#include <stdio.h>

int main(void)
{
    Display *display = XOpenDisplay(NULL);
    AvpPcIdleInhibit *inhibit;
    if (!display) {
        fprintf(stderr, "SKIP: no X11 display (use xvfb-run)\n");
        return 77;
    }
    inhibit = avp_pc_idle_inhibit_create(display);
    if (!inhibit) {
        XCloseDisplay(display);
        return 1;
    }
    avp_pc_idle_inhibit_set_active(inhibit, 1);
    avp_pc_idle_inhibit_set_active(inhibit, 1); /* never double-acquire */
    avp_pc_idle_inhibit_tick(inhibit);
    avp_pc_idle_inhibit_set_active(inhibit, 0);
    avp_pc_idle_inhibit_set_active(inhibit, 0); /* never double-release */
    avp_pc_idle_inhibit_set_active(inhibit, 1);
    avp_pc_idle_inhibit_tick(inhibit);
    avp_pc_idle_inhibit_destroy(inhibit); /* implicit release */
    XCloseDisplay(display);
    puts("PASS: X11 desktop idle inhibition acquire/release lifecycle");
    return 0;
}
