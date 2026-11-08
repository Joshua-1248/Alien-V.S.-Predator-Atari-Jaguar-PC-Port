#ifndef AVP_JAGUAR_HOST_WIDESCREEN_H
#define AVP_JAGUAR_HOST_WIDESCREEN_H
#include <stdint.h>

/* Pure PC presentation geometry. Jaguar gameplay and the original 320x228
 * 4:3 renderer remain unchanged. A Jaguar raster is displayed with its
 * original non-square pixel aspect, so an expanded HOST 4:3 center occupies
 * (height * 4 / 3) presentation pixels. */
static inline unsigned avp_host_wide_native_width(unsigned region_w,unsigned region_h)
{
    uint64_t w;
    if(!region_w||!region_h)return 320u;
    w=((uint64_t)region_w*228u+region_h/2u)/region_h;
    if(w<320u)return 320u;
    return (unsigned)(w>1280u?1280u:w);
}
static inline int avp_host_has_extra_columns(unsigned w,unsigned h)
{
    return w && h && (uint64_t)w*228u>(uint64_t)h*320u;
}
static inline unsigned avp_host_wide_center_width(unsigned h)
{
    return (unsigned)(((uint64_t)h*4u+1u)/3u);
}
static inline double avp_host_wide_focal(unsigned h)
{
    return (2.0/3.0)*(double)h;
}
#endif
