#ifndef AVP_JAGUAR_AVP_GPU_WALL_RASTER_DDA_H
#define AVP_JAGUAR_AVP_GPU_WALL_RASTER_DDA_H
#include <stdint.h>

/* Host-only integer DDA for MAZE2 columns with projected height >=128.
 * Equivalent to trunc(((2*screen_row+1)*128)/(2*wall_h)), including signed
 * division toward zero when a very near wall extends above the viewport.
 * The quotient advances with each displayed pixel without an integer divide.
 * Do not change the Jaguar-derived projection or source-coordinate rules. */
typedef struct AvpWallRowDda {
    int64_t quotient;
    int64_t remainder;
    int64_t denominator;
} AvpWallRowDda;

static inline AvpWallRowDda avp_wall_row_dda_begin(int first_screen_row,int wall_h)
{
    const int64_t numerator=((int64_t)first_screen_row*2+1)*128;
    const int64_t denominator=(int64_t)wall_h*2;
    AvpWallRowDda a={numerator/denominator,numerator%denominator,denominator};
    if(a.remainder<0){--a.quotient;a.remainder+=denominator;}
    return a;
}
static inline int avp_wall_row_dda_value(const AvpWallRowDda *a)
{
    return (int)(a->quotient+(a->quotient<0 && a->remainder!=0));
}
static inline void avp_wall_row_dda_advance(AvpWallRowDda *a)
{
    a->remainder+=256;
    if(a->remainder>=a->denominator){
        a->remainder-=a->denominator;
        ++a->quotient;
    }
}
#endif
