/* S42: verified $F3E6-$F548 all-equal triple-cache early return.
 * This does not implement the cache-miss Blitter path at $F41C. */
#ifndef AVP_JAG_PANEL_TRIPLE_CACHED_H
#define AVP_JAG_PANEL_TRIPLE_CACHED_H
#include "jaguar/avp_gpu/view3d.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded successful early-return only. On a cache mismatch, returns zero
 * WITHOUT mutating memory: caller must use the original Blitter path.
 * `index` is read from original Jaguar DRAM $2F5B6. */
int avp_jagpanel_triple_cached_reuse(const AvpView3DBus *bus);
#ifdef __cplusplus
}
#endif
#endif
