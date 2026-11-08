#include "jaguar/avp_gpu/maze2_column.h"
#include <stdint.h>
#include <limits.h>
#include <math.h>

/* MAZE2.GAS::maze_scan evaluates the top-wall test strictly before the
 * unconditional right-wall crossing.  At a shared corner it therefore
 * crosses the right boundary; selecting top instead samples a different
 * authored wall and can expose the wrong face in narrow ducts. */
int avp_gpu_maze2_right_wall_first(double right_distance,double top_distance)
{
    return right_distance<=top_distance;
}

/* DrawColumn derives the full projected sprite height from y_inc and then
 * applies the screen clip.  The old host 4*height limit altered source texel
 * selection near walls, most noticeably in the duct levels. */
int avp_gpu_maze2_project_wall_height(double distance,unsigned framebuffer_height)
{
    double height;
    if(!framebuffer_height)return 0;
    if(!(distance>=0.001))distance=0.001;
    height=ceil((128.0/distance)*((double)framebuffer_height/228.0));
    if(!isfinite(height)||height>(double)(INT_MAX/4))return INT_MAX/4;
    return (int)height;
}


/* MAZE2.GAS::common_plain: (unclipped_height+1)>>1 and then
 * draw_start=-half, draw_end=+half.  Floor-dividing only the top in
 * the old host left odd-height walls a pixel off centre at duct seams. */
int avp_gpu_maze2_centered_wall_bounds(unsigned fb_height,int projected_height,int *top,int *bottom)
{
    int mid,half;
    if(!top||!bottom||!fb_height||projected_height<=0)return 0;
    mid=(int)(fb_height/2u);
    half=(projected_height/2)+(projected_height&1);
    *top=mid-half;
    *bottom=mid+half;
    return 1;
}

u32 avp_gpu_maze2_y_inc_from_z(u32 z)
{
    u32 y_inc=z;
    y_inc+=z>>2;
    y_inc&=~1u;
    return y_inc;
}

int avp_gpu_maze2_draw_half(u32 y_inc)
{
    u32 q;
    if(!y_inc)return 0;
    q=(u32)(((uint64_t)(64u<<16)*2u)/y_inc);
    return (int)((q+1u)>>1);
}

int avp_gpu_maze2_scale_edge(s8 sprite_edge,u32 y_inc)
{
    int edge=(int)sprite_edge;
    u32 mag,q;
    if(!y_inc||edge==0)return 0;
    mag=(u32)(edge<0?-edge:edge);
    q=(u32)(((uint64_t)mag<<17)/y_inc);
    q=(q+1u)>>1;
    return edge<0?-(int)q:(int)q;
}

u32 avp_gpu_maze2_source_y_16_16(int screen_y,u32 y_inc)
{
    int64_t odd=(int64_t)screen_y*2+1;
    int64_t prod=odd*(int64_t)y_inc;
    int64_t delta,src;
    if(prod<0)delta=-(int64_t)(((uint64_t)(-prod))>>1);
    else delta=(int64_t)(((uint64_t)prod)>>1);
    src=((int64_t)64<<16)+delta;
    return (u32)src;
}

/* MAZE2.GAS octant transforms (Q_ENE..Q_ESE) and xor tommy in
 * hit_right/hit_top produce an oriented sprite coordinate before the
 * independent bit-7 authored reflection in DrawColumn.  West-facing
 * vertical hits and south-facing horizontal hits reverse the cell fraction. */
double avp_gpu_maze2_orient_fraction(double frac,int vertical,int step)
{
    return ((vertical&&step<0)||(!vertical&&step>0))?1.0-frac:frac;
}

int avp_gpu_maze2_singles_source_col(int post_reflect_col,int reflected,int *source_col)
{
    int col=post_reflect_col&127;
    if(col&0x40)return 0;
    if(reflected)col^=0x7f;
    if(source_col)*source_col=col;
    return 1;
}

/* Retail common_win collapses the scan window if its signed edges
 * coincide.  A reversed/empty window cannot admit farther wall rays. */
int avp_gpu_maze2_window_pair_transmits(const u8 *window_pairs,unsigned col)
{
    if(!window_pairs || col>=128u)return 0;
    return (s8)window_pairs[2u*col] < (s8)window_pairs[2u*col+1u];
}

/* One screen-column's surviving window, intersected in front-to-back
 * order like the GPU's clip_top and clip_bottom.  Coordinates are already
 * in destination pixels; the caller clamps to the framebuffer. */
int avp_gpu_maze2_clip_intersect(int *lo,int *hi,int opening_lo,int opening_hi)
{
    if(!lo||!hi)return 0;
    if(opening_lo>*lo)*lo=opening_lo;
    if(opening_hi<*hi)*hi=opening_hi;
    if(*lo>=*hi){*lo=0;*hi=0;return 0;}
    return 1;
}

/* Return the *uncovered* source-space interval for MAZE2 vertical doors.
 * The host column drawer already uses exactly these edge formulae; visibility
 * must use them too, or a closed door becomes a transparent portal.
 * Types 9-11 additionally intersect with the authored bulkhead window. */
int avp_gpu_maze2_vertical_door_aperture(unsigned type,u8 open,
                                          int window_top,int window_bottom,
                                          int *top,int *bottom)
{
    int lo,hi;
    if(!top||!bottom||type<7u||type>11u)return 0;
    if(type==7u){lo=64-(int)open;hi=64;}
    else if(type==8u){lo=-(int)open;hi=(int)open;}
    else {
        if(window_top<-64)window_top=-64;
        if(window_top>64)window_top=64;
        if(window_bottom<-64)window_bottom=-64;
        if(window_bottom>64)window_bottom=64;
        if(window_bottom<=window_top)return 0;
        if(type==9u||type==10u){lo=window_bottom-(int)open;hi=window_bottom;}
        else{lo=12-(int)open;hi=12+(int)open;}
        if(lo<window_top)lo=window_top;
        if(hi>window_bottom)hi=window_bottom;
    }
    if(lo<-64)lo=-64;
    if(hi>64)hi=64;
    if(lo>=hi)return 0;
    *top=lo;*bottom=hi;
    return 1;
}

static int ceil_div_pos(int n,int d)
{
    return n<=0?0:(n+d-1)/d;
}

int avp_gpu_maze2_host_edge_y(int base_y0,int wall_h,int signed_source_y)
{
    int top_half,bottom_half,centre,mag,off;
    if(wall_h<=0)return base_y0;
    if(signed_source_y<=-64)return base_y0;
    if(signed_source_y>=64)return base_y0+wall_h;
    top_half=wall_h/2;
    bottom_half=wall_h-top_half;
    centre=base_y0+top_half;
    if(signed_source_y<0){mag=-signed_source_y;off=ceil_div_pos(mag*top_half,64);return centre-off;}
    if(signed_source_y>0){off=ceil_div_pos(signed_source_y*bottom_half,64);return centre+off;}
    return centre;
}

int avp_gpu_maze2_host_source_row(int screen_row,int wall_h,int source_y_shift)
{
    int64_t num,den;
    int row;
    if(wall_h<=0)return 0;
    num=((int64_t)screen_row*2+1)*128;
    den=(int64_t)wall_h*2;
    if(num<0)row=-(int)((-num)/den);
    else row=(int)(num/den);
    row+=source_y_shift;
    if(row<0)row=0;
    if(row>127)row=127;
    return row;
}

int avp_gpu_maze2_centered_door_source_col(int sprite_col,u8 open,int *source_col)
{
    int col=sprite_col&127;
    int shifted=col+((col&0x40)?-(int)open:(int)open);
    if((col^shifted)&0x40)return 0;
    if(source_col)*source_col=shifted;
    return 1;
}

/* MAZE2.GAS csb_draw / csp_draw / csw_draw use the sliding half-panel
 * test for all three, but have different meanings for an uncovered column:
 *  csb: common_win on the fixed bulkhead; a covered column is opaque.
 *  csp: uncovered is empty; covered is fully opaque.
 *  csw: uncovered is empty; covered still has the moved panel's window.
 * A zero-height or missing internal window must never open the scan. */
int avp_gpu_maze2_centered_visibility(unsigned type,u8 open,int sprite_col,
                                       const u8 *window_pairs)
{
    int shifted,covered,window_col;
    if(type<4u||type>6u)return 0;
    sprite_col&=127;
    covered=avp_gpu_maze2_centered_door_source_col(sprite_col,open,&shifted);
    if(type==5u)return covered?0:1;
    if(type==4u){
        if(covered)return 0;
        window_col=sprite_col;
    }else{
        if(!covered)return 1;
        window_col=shifted;
    }
    return avp_gpu_maze2_window_pair_transmits(window_pairs,(unsigned)window_col)?2:0;
}
