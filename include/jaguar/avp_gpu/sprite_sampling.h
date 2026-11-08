#ifndef AVP_GPU_SPRITE_SAMPLING_H
#define AVP_GPU_SPRITE_SAMPLING_H

#include <math.h>

/* For a Jaguar OP nearest-neighbour scaled sprite, a destination pixel
 * samples exactly one source texel at its centre.  Rectangle-expanding
 * each individual source texel instead creates overlapping coverage when
 * the object is minified, which corrupts small impact animations. */
static inline int avp_gpu_sprite_sample_coord(int destination,
                                               double projected_origin,
                                               double scale)
{
    return scale>0.0 ? (int)floor(((double)destination+0.5-projected_origin)/scale) : -1;
}

/* Clamp a projected texel rectangle to the host framebuffer before scanning.
 * Former offscreen iterations were all skipped in the inner loop; the
 * remaining on-screen pixel-centre samples and their order are unchanged. */
static inline int avp_gpu_sprite_clip_span(int *first,int *last,int size)
{
    if(*first<0)*first=0;
    if(*last>size)*last=size;
    return *first<*last;
}

#endif
