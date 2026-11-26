#ifndef AVP_PC_IDLE_INHIBIT_H
#define AVP_PC_IDLE_INHIBIT_H

/* Linux/X11 host-only desktop idle inhibition. No effect on Jaguar gameplay.
 * One object belongs to one open X11 display and must be destroyed before it.
 */
#ifndef _WIN32
#include <X11/Xlib.h>
typedef struct AvpPcIdleInhibit AvpPcIdleInhibit;
AvpPcIdleInhibit *avp_pc_idle_inhibit_create(Display *display);
void avp_pc_idle_inhibit_set_active(AvpPcIdleInhibit *inhibit, int active);
void avp_pc_idle_inhibit_tick(AvpPcIdleInhibit *inhibit);
void avp_pc_idle_inhibit_destroy(AvpPcIdleInhibit *inhibit);
#endif
#endif
