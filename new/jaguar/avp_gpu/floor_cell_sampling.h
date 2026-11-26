#ifndef AVP_GPU_FLOOR_CELL_SAMPLING_H
#define AVP_GPU_FLOOR_CELL_SAMPLING_H

/* The projected floor/ceiling world coordinates need a nonnegative maze
 * cell before any texture can be drawn.  For finite coordinates inside the
 * map, conversion to int is exactly floor(): all values are >= 0 and
 * the 64x64 Jaguar maze dimensions fit in int.  Reject other coordinates
 * before conversion to avoid two libm floor calls at every host pixel.
 * The screen-space ray increments and fractional texture coordinates stay
 * unchanged.  This is host-only draw-loop work, not collision/world logic. */
static inline int avp_gpu_floor_valid_cell(double wx,double wy,
                                            unsigned mw,unsigned mh,
                                            int *cx,int *cy)
{
    if(!(wx>=0.0 && wy>=0.0 && wx<(double)mw && wy<(double)mh))
        return 0;
    *cx=(int)wx;
    *cy=(int)wy;
    return 1;
}

#endif
