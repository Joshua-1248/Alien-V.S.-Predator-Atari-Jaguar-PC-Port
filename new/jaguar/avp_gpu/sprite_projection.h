#ifndef AVP_GPU_SPRITE_PROJECTION_H
#define AVP_GPU_SPRITE_PROJECTION_H
/* All AAF world sprites use the SAME horizontal picture-plane projection
 * as wall rays.  A widescreen field of view adds columns outside the original
 * central 4:3 field; it does not increase the apparent width of an enemy.
 *
 * At the legacy 4:3 sizes the wall focal is exactly framebuffer_width/2,
 * preserving the previous scaling calculation bit for bit.  In widescreen
 * the wall focal is based on framebuffer HEIGHT, not the extra columns.
 * The independent Y ratio is retained for Jaguar's non-square pixels. */
static inline void avp_gpu_sprite_viewport_scale(unsigned width,unsigned height,
                                               double focal,double distance,
                                               int retail_rescale,
                                               double *scale_x,double *scale_y)
{
    (void)width;
    if(scale_x)*scale_x=(focal/160.0)/distance*((double)retail_rescale/128.0);
    if(scale_y)*scale_y=((double)height/228.0)/distance*((double)retail_rescale/128.0);
}
#endif
