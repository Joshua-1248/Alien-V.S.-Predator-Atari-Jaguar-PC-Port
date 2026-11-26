#ifndef AVP_JAGUAR_AVP_GPU_TRACKER_CONTACT_MATH_H
#define AVP_JAGUAR_AVP_GPU_TRACKER_CONTACT_MATH_H

#include <math.h>
/* Original MAZE4.GAS motion tracker: 3m per map tile, approximated
 * diagonal range max(|x|,|y|)+min(|x|,|y|)/3.  Keep the original strict
 * 15m scanner clipping for both AMP and multiplayer player contacts. */
static inline double avp_gpu_tracker_contact_metres(double lateral_tiles,
                                                    double forward_tiles)
{
    const double x=fabs(lateral_tiles*3.0),y=fabs(forward_tiles*3.0);
    return (x>y?x:y)+(x>y?y:x)/3.0;
}
#endif
