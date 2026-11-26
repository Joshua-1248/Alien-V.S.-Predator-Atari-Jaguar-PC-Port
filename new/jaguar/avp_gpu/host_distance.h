#ifndef AVP_JAGUAR_AVP_GPU_HOST_DISTANCE_H
#define AVP_JAGUAR_AVP_GPU_HOST_DISTANCE_H
/* PC presentation-only enhancements.  Zero preserves the original
 * eight-room ray scan and original Jaguar CRY distance shading.
 * Choices are kept separate from gameplay, weapon range and map reveal. */
#define AVP_HOST_DRAW_DISTANCE_CHOICES 5
#define AVP_HOST_FOG_DISTANCE_CHOICES 4
static inline int avp_host_distance_cycle(int value,int direction,int choices)
{
    if(choices<=0)return 0;
    if(value<0||value>=choices)value=0;
    return (value+(direction>=0?1:choices-1))%choices;
}
static inline double avp_host_ray_limit(int draw_mode)
{
    static const double limits[AVP_HOST_DRAW_DISTANCE_CHOICES]={8.0,16.0,24.0,32.0,128.0};
    /* A Jaguar maze is at most 64 by 64 cells: 128 rooms exceeds the
     * greatest possible diagonal. The raycaster also caps DDA steps at 128. */
    return limits[draw_mode>=0&&draw_mode<AVP_HOST_DRAW_DISTANCE_CHOICES?draw_mode:0];
}
/* Host floor/ceiling row visibility.  The original Jaguar eight-room
 * presentation does not draw retail scanlines [106,122); retain those exact
 * rows for mode zero.  Extended modes must also extend the flat horizon:
 * the former fixed skip otherwise capped visible flats near eight rooms
 * regardless of the wall draw distance.  Projection is 64 / horizon offset
 * in 228-line Jaguar coordinates, including high-resolution and split views.
 *
 * Do not pin out-of-range rows to the maximum depth: that would repeat a
 * distant flat texel across the horizon instead of showing actual geometry.
 * A half-retail-scanline lower bound avoids division by zero exactly on the
 * horizon; rows beyond the selected limit remain unpainted. */
static inline int avp_host_flat_row_depth(double retail_row,int draw_mode,
                                          double *depth)
{
    double off=retail_row-114.0;
    double dist,max_depth;
    if(!depth)return 0;
    if(off<0.0)off=-off;
    if(draw_mode<=0 || draw_mode>=AVP_HOST_DRAW_DISTANCE_CHOICES){
        if(retail_row>=106.0 && retail_row<122.0)return 0;
        if(off<0.5)off=0.5;
        dist=64.0/off;
        if(dist>8.0)dist=8.0;
        *depth=dist;
        return 1;
    }
    if(off<0.5)return 0;
    dist=64.0/off;
    max_depth=avp_host_ray_limit(draw_mode);
    if(dist>max_depth)return 0;
    *depth=dist;
    return 1;
}
static inline double avp_host_fog_distance(double rooms,int fog_mode)
{
    /* Keep the original CRY shade function and extend its distance domain.
     * A separate Video option controls whether to bypass distance shading. */
    static const double reciprocal[AVP_HOST_FOG_DISTANCE_CHOICES]={1.0,0.5,1.0/3.0,0.25};
    return rooms*reciprocal[fog_mode>=0&&fog_mode<AVP_HOST_FOG_DISTANCE_CHOICES?fog_mode:0];
}
static inline int avp_host_fog_shade_delta(int retail_delta,int disable_fog)
{
    return disable_fog?0:retail_delta;
}
static inline const char *avp_host_draw_distance_label(int mode)
{
    static const char *labels[]={"8 rooms (Jaguar)","16 rooms","24 rooms","32 rooms","Unlimited (whole level)"};
    return labels[mode>=0&&mode<AVP_HOST_DRAW_DISTANCE_CHOICES?mode:0];
}
static inline const char *avp_host_fog_distance_label(int mode)
{
    static const char *labels[]={"Original (Jaguar)","2x distance","3x distance","4x distance"};
    return labels[mode>=0&&mode<AVP_HOST_FOG_DISTANCE_CHOICES?mode:0];
}
#endif
