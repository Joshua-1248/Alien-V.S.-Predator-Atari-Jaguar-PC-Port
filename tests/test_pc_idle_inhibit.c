/* Native X11 integration smoke test; run with xvfb-run or a desktop DISPLAY.
 * The test verifies repeated focus transitions and cleanup do not crash;
 * an actual desktop session daemon is needed to prove idle-lock suppression.
 */
#include "pc_idle_inhibit.h"
#include <stdio.h>
#include <dlfcn.h>

int main(void)
{
    /* Test-only: keep libdbus mapped through the isolated process-global
     * shutdown. The actual game must not force shutdown on other D-Bus users. */
    void *dbus_library = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    Display *display = XOpenDisplay(NULL);
    AvpPcIdleInhibit *inhibit;
    if (!display) {
        if (dbus_library) dlclose(dbus_library);
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
    /* libdbus is intentionally process-global. Free its global state only
     * in this standalone test, after the application's private connection
     * has been released; never make this a game destructor side effect. */
    if (dbus_library) {
        void (*shutdown_fn)(void) = NULL;
        *(void **)&shutdown_fn = dlsym(dbus_library, "dbus_shutdown");
        if (shutdown_fn) shutdown_fn();
        dlclose(dbus_library);
    }
    puts("PASS: X11 desktop idle inhibition acquire/release lifecycle");
    return 0;
}
