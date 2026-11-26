/* RE25-S43: original $F3E6-$F548 three-pass Blitter cache-mismatch path.
 * Isolated verification-only code. Uses the caller's original Jaguar bus and
 * Blitter register implementation; it does NOT implement a host pixel copy.
 * No live PC gameplay/renderer path calls this function. */
#ifndef AVP_JAG_PANEL_TRIPLE_REBUILD_H
#define AVP_JAG_PANEL_TRIPLE_REBUILD_H
#include "jaguar/avp_gpu/view3d.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Only a bounded set of original 32-KiB slots wholly within Jaguar DRAM is
 * supported. `blitter_ready` explicitly declares that reads from $F02238
 * report the hardware idle bit and writes execute Jaguar Blitter commands.
 * All-equal cache hits and unsupported ranges return 0 without writes.
 * A mismatch issues the original three commands and returns 1.
 * This is NOT a cycle-exact hardware arbitration model. */
int avp_jagpanel_triple_rebuild(const AvpView3DBus *bus, int blitter_ready);
#ifdef __cplusplus
}
#endif
#endif
