#ifndef AVP_MICRO_POINT_FLOOR_ROW_H
#define AVP_MICRO_POINT_FLOOR_ROW_H
#include "jaguar/avp_gpu/host_flat_shading.h"
#include "jaguar/avp_gpu/floor_cell_sampling.h"
#include <math.h>
#include <string.h>
/* Optional unfiltered floor/ceiling raster: process the pixels inside one
 * cell as a span. Keep the original repeated double additions (including
 * rounding at cell boundaries), texel truncation, rotation and live map
 * bytes. No geometry or texture samples are cached across rows/frames. */
static inline void avp_micro_point_floor_row_impl(u32 *row,unsigned width,
    const u8 *maze,unsigned mw,unsigned mh,unsigned ceiling,
    double wx,double wy,double dx,double dy,
    const AvpGpuRgbImage *const textures[256],const unsigned rotations[256],
    const u8 shades[256],const u32 cry_rgb[65536],int clear_missing)
{
    unsigned x=0;
    const int monotone=isfinite(dx)&&isfinite(dy);
    const unsigned fixed_axes=(dx==0.0?1u:0u)|(dy==0.0?2u:0u);
    const unsigned direction=(dx<0.0?1u:0u)|(dy<0.0?2u:0u);
    while(x<width){
        int cx,cy;
        if(!avp_gpu_floor_valid_cell(wx,wy,mw,mh,&cx,&cy)){
            /* A finite, monotone ray cannot re-enter after leaving a
             * boundary in its direction of travel. Preserve untouched pixels. */
            if(monotone && ((wx<0.0 && dx<=0.0) || (wx>=(double)mw && dx>=0.0) ||
                (wy<0.0 && dy<=0.0) || (wy>=(double)mh && dy>=0.0))){
                if(clear_missing)memset(row+x,0,(size_t)(width-x)*sizeof(*row));
                return;
            }
            if(clear_missing)row[x]=0;
            ++x;wx+=dx;wy+=dy;continue;
        }
        const unsigned flat=maze[((size_t)cy*mw+(unsigned)cx)*8u+(ceiling?4u:5u)];
        const AvpGpuRgbImage *t=textures[flat];
        const double minx=(double)cx,miny=(double)cy,maxx=minx+1.0,maxy=miny+1.0;
#define AVP_MICRO_ADVANCE() do { ++x;wx+=dx;wy+=dy; } while(0)
#define AVP_MICRO_IN_CELL() (x<width && wx>=minx && wx<maxx && wy>=miny && wy<maxy)
        if(!t){
            const unsigned first=x;
#if defined(__linux__) && defined(__clang__)
            if(monotone){
                if(dx==0.0 && dy==0.0){
                    if(clear_missing)memset(row+x,0,(size_t)(width-x)*sizeof(*row));
                    return;
                }
                while(width-x>=16u){
                    double nx=wx,ny=wy;
                    for(unsigned j=0;j<16u;j++){nx+=dx;ny+=dy;}
                    if(!(nx>=minx && nx<maxx && ny>=miny && ny<maxy))break;
                    wx=nx;wy=ny;x+=16u;
                }
            }
            if(x<width){do { AVP_MICRO_ADVANCE(); } while(AVP_MICRO_IN_CELL());}
#elif defined(__linux__) && defined(__GNUC__) && !defined(__clang__)
            /* R445: a finite monotone ray stays within this
             * rectangular cell if the eighth successive sample does.
             * Do not replace the repeated binary64 additions with a
             * multiplication. The scalar tail handles all boundary steps. */
            if(monotone){
                while(width-x>=8u){
                    double nx=wx,ny=wy;
                    for(unsigned j=0;j<8u;j++){nx+=dx;ny+=dy;}
                    if(!(nx>=minx && nx<maxx && ny>=miny && ny<maxy))break;
                    wx=nx;wy=ny;x+=8u;
                    if(x==width)break;
                }
            }
            if(x<width){do { AVP_MICRO_ADVANCE(); } while(AVP_MICRO_IN_CELL());}
#else
            do { AVP_MICRO_ADVANCE(); } while(AVP_MICRO_IN_CELL());
#endif
            if(clear_missing)memset(row+first,0,(size_t)(x-first)*sizeof(*row));
            continue;
        }
        const int tw=t->width,th=t->height;
        if(tw<=0||th<=0){do { row[x]=0x005a462du;AVP_MICRO_ADVANCE(); } while(AVP_MICRO_IN_CELL());continue;}
        const unsigned rot=tw==th?rotations[flat]:0u;
        if(t->cry16 && !rot){
            const u16 *tex=t->cry16;
            /* Once inside a cell, a finite monotone coordinate only
             * needs the boundary in its direction of travel. Keep the
             * exact per-pixel double additions and texture conversions. */
#define AVP_MICRO_CRY_SPAN(condition,INDEX) do { \
                const u16 c=tex[INDEX]; \
                row[x]=cry_rgb[(c&0xff00u)|shades[c&255u]]; \
                AVP_MICRO_ADVANCE(); \
            } while(x<width && (condition))
#define AVP_MICRO_CRY_DIRECTIONAL(INDEX) do { \
            switch(direction){ \
            case 0u: AVP_MICRO_CRY_SPAN(wx<maxx && wy<maxy,INDEX);break; \
            case 1u: AVP_MICRO_CRY_SPAN(wx>=minx && wy<maxy,INDEX);break; \
            case 2u: AVP_MICRO_CRY_SPAN(wx<maxx && wy>=miny,INDEX);break; \
            default: AVP_MICRO_CRY_SPAN(wx>=minx && wy>=miny,INDEX);break; \
            } } while(0)
            /* Original 128x128 CRY flats: multiplication by a power of two
             * is exact and subtracting the integral cell is exact here.
             * Mask the truncated nonnegative scaled world coordinate to
             * obtain the same texel, without subtracting the cell or using
             * a variable texture stride. Bound the map for signed casts. */
#define AVP_MICRO_INDEX_128 (((unsigned)(int)(wy*128.0)&127u)*128u+((unsigned)(int)(wx*128.0)&127u))
#define AVP_MICRO_INDEX_GENERIC ((size_t)(unsigned)((wy-miny)*th)*(unsigned)tw+(unsigned)((wx-minx)*tw))
            if(!monotone){AVP_MICRO_CRY_SPAN(AVP_MICRO_IN_CELL(),AVP_MICRO_INDEX_GENERIC);}
            else if(tw==128 && th==128 && mw<=16777215u && mh<=16777215u){
                /* A cardinal ray keeps one texel coordinate invariant.
                 * Hoist that conversion and the invariant cell bound out
                 * of the span. Keep repeated additions for the moving axis. */
                /* R432: scaling a normal binary64 coordinate and its step
                 * by 2^7 commutes with correctly rounded addition, while
                 * the bounded world coordinate avoids overflow. Use the
                 * original path for very large or tiny steps. The moving
                 * coordinate is restored at each cell boundary. */
#define AVP_MICRO_SCALED_SPAN(COND,INDEX) do { \
                    const u16 c=tex[INDEX]; \
                    row[x++]=cry_rgb[(c&0xff00u)|shades[c&255u]]; \
                    q+=dq; \
                } while(x<width && (COND))
                if(fixed_axes==2u){
                    const unsigned base=((unsigned)(int)(wy*128.0)&127u)*128u;
                    if(fabs(dx)>=0x1p-32 && fabs(dx)<=16777216.0){
                        double q=wx*128.0,dq=dx*128.0;
                        if(direction&1u){const double bound=minx*128.0;AVP_MICRO_SCALED_SPAN(q>=bound,base+((unsigned)(int)q&127u));}
                        else{const double bound=maxx*128.0;AVP_MICRO_SCALED_SPAN(q<bound,base+((unsigned)(int)q&127u));}
                        wx=q*0x1p-7;
                    }else if(direction&1u){AVP_MICRO_CRY_SPAN(wx>=minx,base+((unsigned)(int)(wx*128.0)&127u));}
                    else{AVP_MICRO_CRY_SPAN(wx<maxx,base+((unsigned)(int)(wx*128.0)&127u));}
                }else if(fixed_axes==1u){
                    const unsigned column=(unsigned)(int)(wx*128.0)&127u;
                    if(fabs(dy)>=0x1p-32 && fabs(dy)<=16777216.0){
                        double q=wy*128.0,dq=dy*128.0;
                        if(direction&2u){const double bound=miny*128.0;AVP_MICRO_SCALED_SPAN(q>=bound,((unsigned)(int)q&127u)*128u+column);}
                        else{const double bound=maxy*128.0;AVP_MICRO_SCALED_SPAN(q<bound,((unsigned)(int)q&127u)*128u+column);}
                        wy=q*0x1p-7;
                    }else if(direction&2u){AVP_MICRO_CRY_SPAN(wy>=miny,((unsigned)(int)(wy*128.0)&127u)*128u+column);}
                    else{AVP_MICRO_CRY_SPAN(wy<maxy,((unsigned)(int)(wy*128.0)&127u)*128u+column);}
#undef AVP_MICRO_SCALED_SPAN
                }else if(fixed_axes==3u){
                    const u16 c=tex[AVP_MICRO_INDEX_128];
                    const u32 color=cry_rgb[(c&0xff00u)|shades[c&255u]];
                    for(;x<width;++x)row[x]=color;
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
                }else if(fabs(dx)>=0x1p-32 && fabs(dx)<=16777216.0 &&
                         fabs(dy)>=0x1p-32 && fabs(dy)<=16777216.0){
                    /* R435: restore R433 with revised floor worker scheduling;
                     * extend the exact power-of-two cardinal scaling
                     * to both moving coordinates of oblique retail flats. */
                    double qx=wx*128.0,qy=wy*128.0;
                    const double sx=dx*128.0,sy=dy*128.0;
                    const double bx=(direction&1u?minx:maxx)*128.0;
                    const double by=(direction&2u?miny:maxy)*128.0;
#define AVP_MICRO_OBLIQUE_SCALED(COND) do { \
                        const unsigned index=(((unsigned)(int)qy&127u)<<7)|((unsigned)(int)qx&127u); \
                        const u16 c=tex[index]; \
                        row[x++]=cry_rgb[(c&0xff00u)|shades[c&255u]]; \
                        qx+=sx;qy+=sy; \
                    } while(x<width && (COND))
                    switch(direction){
                    case 0u: AVP_MICRO_OBLIQUE_SCALED(qx<bx && qy<by);break;
                    case 1u: AVP_MICRO_OBLIQUE_SCALED(qx>=bx && qy<by);break;
                    case 2u: AVP_MICRO_OBLIQUE_SCALED(qx<bx && qy>=by);break;
                    default: AVP_MICRO_OBLIQUE_SCALED(qx>=bx && qy>=by);break;
                    }
#undef AVP_MICRO_OBLIQUE_SCALED
                    wx=qx*0x1p-7;wy=qy*0x1p-7;
#endif
                }else{AVP_MICRO_CRY_DIRECTIONAL(AVP_MICRO_INDEX_128);}
            }else{AVP_MICRO_CRY_DIRECTIONAL(AVP_MICRO_INDEX_GENERIC);}
#undef AVP_MICRO_INDEX_GENERIC
#undef AVP_MICRO_INDEX_128
#undef AVP_MICRO_CRY_DIRECTIONAL
#undef AVP_MICRO_CRY_SPAN
#if (defined(__GNUC__) && !defined(__clang__)) || defined(AVP_MICRO_TEST_ROTATED_FLOOR)
        }else if(t->cry16 && rot<=3u && tw==128 && th==128 && monotone &&
                 mw<=16777215u && mh<=16777215u && fixed_axes){
            /* Cardinal rays on rotated retail flats: exactly one source
             * coordinate moves. Hoist the other and the rotation out of
             * the span, while retaining the original double additions. */
            const u16 *tex=t->cry16;
            unsigned u=(unsigned)(int)(wx*128.0)&127u;
            unsigned v=(unsigned)(int)(wy*128.0)&127u;
            unsigned base=0u,shift=0u,flip=0u;
            if(fixed_axes==3u){
                unsigned i=rot==1u?((127u-u)<<7)+v:rot==2u?((127u-v)<<7)+(127u-u):(u<<7)+(127u-v);
                const u16 c=tex[i];const u32 color=cry_rgb[(c&0xff00u)|shades[c&255u]];
                for(;x<width;++x)row[x]=color;
                continue;
            }
            if(fixed_axes==2u){ /* u varies, v fixed */
                if(rot==1u){base=v;shift=7u;flip=127u;}
                else if(rot==2u){base=(127u-v)<<7;shift=0u;flip=127u;}
                else{base=127u-v;shift=7u;flip=0u;}
            }else{ /* v varies, u fixed */
                if(rot==1u){base=(127u-u)<<7;shift=0u;flip=0u;}
                else if(rot==2u){base=127u-u;shift=7u;flip=127u;}
                else{base=u<<7;shift=0u;flip=127u;}
            }
#define AVP_ROT_FIXED_SPAN(COORD,COND) do { \
                const unsigned moving=((unsigned)(int)((COORD)*128.0)&127u)^flip; \
                const u16 c=tex[base+(moving<<shift)]; \
                row[x]=cry_rgb[(c&0xff00u)|shades[c&255u]]; \
                AVP_MICRO_ADVANCE(); \
            } while(x<width && (COND))
            if(fixed_axes==2u){
                if(direction&1u){AVP_ROT_FIXED_SPAN(wx,wx>=minx);}
                else{AVP_ROT_FIXED_SPAN(wx,wx<maxx);}
            }else{
                if(direction&2u){AVP_ROT_FIXED_SPAN(wy,wy>=miny);}
                else{AVP_ROT_FIXED_SPAN(wy,wy<maxy);}
            }
#undef AVP_ROT_FIXED_SPAN
#endif
        }else{
            do {
                int tx=(int)((wx-minx)*tw),ty=(int)((wy-miny)*th);
                const int oldx=tx,oldy=ty;
                switch(rot){case 1u:tx=oldy;ty=th-1-oldx;break;
                    case 2u:tx=tw-1-oldx;ty=th-1-oldy;break;
                    case 3u:tx=tw-1-oldy;ty=oldx;break;default:break;}
                row[x]=avp_gpu_host_flat_sample_inbounds(t,(unsigned)tx,(unsigned)ty,shades,cry_rgb);
                AVP_MICRO_ADVANCE();
            } while(AVP_MICRO_IN_CELL());
        }
#undef AVP_MICRO_ADVANCE
#undef AVP_MICRO_IN_CELL
    }
}
/* The legacy helper retains its skip semantics for callers that already
 * initialized the framebuffer; full-resolution host rendering can instead
 * initialize only holes, so every destination pixel is written exactly once
 * by the floor pass. Row workers own disjoint destination rows. */
static inline void avp_micro_point_floor_row(u32 *row,unsigned width,
    const u8 *maze,unsigned mw,unsigned mh,unsigned ceiling,
    double wx,double wy,double dx,double dy,
    const AvpGpuRgbImage *const textures[256],const unsigned rotations[256],
    const u8 shades[256],const u32 cry_rgb[65536])
{
    avp_micro_point_floor_row_impl(row,width,maze,mw,mh,ceiling,wx,wy,dx,dy,
        textures,rotations,shades,cry_rgb,0);
}
/* This path must only be used when every row is owned by the unfiltered
 * textured floor pass. The caller clears rejected horizon/depth rows. */
static inline int avp_micro_floor_defer_color_clear(unsigned w,unsigned h,
    int micro,int textured,int filtered)
{
    return w>=1280u && h>=720u && micro && textured && !filtered;
}
#endif
