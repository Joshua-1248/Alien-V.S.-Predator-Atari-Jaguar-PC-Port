#ifndef AVP_MICRO_DEPTH_CLEAR_FAST_H
#define AVP_MICRO_DEPTH_CLEAR_FAST_H
#include "jaguar/avp_gpu/maze2_occlusion.h"
#include <float.h>
#include <stddef.h>
/* Called by synchronous raster workers. Each worker owns disjoint rows;
 * store the same exact FLT_MAX sentinel used by the existing clear. */
static inline void avp_micro_depth_clear_rows(void *arg,unsigned first,unsigned last)
{
    AvpMaze2Occlusion *o=(AvpMaze2Occlusion*)arg;
    float *d=o->depth+(size_t)first*o->width;
    const size_t n=(size_t)(last-first)*o->width;
    for(size_t i=0;i<n;i++)d[i]=FLT_MAX;
}
#endif
