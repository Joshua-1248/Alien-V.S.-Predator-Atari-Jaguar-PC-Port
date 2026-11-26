#ifndef AVP_JAGUAR_AVP_GPU_MAZE2_COLUMN_H
#define AVP_JAGUAR_AVP_GPU_MAZE2_COLUMN_H

#include "avp_types.h"

/* Retail Z_MAX is 8 rooms (16 with the optional PC extended view).
 * A diagonal ray can cross many grid boundaries; preserve each portal. */
#define AVP_MAZE2_MAX_RAY_PORTALS 48u

#ifdef __cplusplus
extern "C" {
#endif

/* MAZE2 maze_scan resolves exact corner ties at the right boundary. */
int avp_gpu_maze2_right_wall_first(double right_distance,double top_distance);
/* Project the complete source wall before viewport clipping. */
int avp_gpu_maze2_project_wall_height(double distance,unsigned framebuffer_height);
/* DrawColumn rounds the wall half-height outwards around the screen centre. */
int avp_gpu_maze2_centered_wall_bounds(unsigned fb_height,int projected_height,int *top,int *bottom);
u32 avp_gpu_maze2_y_inc_from_z(u32 z);
int avp_gpu_maze2_draw_half(u32 y_inc);
int avp_gpu_maze2_scale_edge(s8 sprite_edge,u32 y_inc);
u32 avp_gpu_maze2_source_y_16_16(int screen_y,u32 y_inc);
int avp_gpu_maze2_singles_source_col(int post_reflect_col,int reflected,int *source_col);
double avp_gpu_maze2_orient_fraction(double frac,int vertical,int step);
int avp_gpu_maze2_window_pair_transmits(const u8 *window_pairs,unsigned col);
int avp_gpu_maze2_clip_intersect(int *lo,int *hi,int opening_lo,int opening_hi);
int avp_gpu_maze2_vertical_door_aperture(unsigned type,u8 open,int window_top,
                                          int window_bottom,int *top,int *bottom);
int avp_gpu_maze2_host_edge_y(int base_y0,int wall_h,int signed_source_y);
int avp_gpu_maze2_host_source_row(int screen_row,int wall_h,int source_y_shift);
int avp_gpu_maze2_centered_door_source_col(int sprite_col,u8 open,int *source_col);
/* Return 0 = solid, 1 = no wall column, 2 = windowed frame with a
 * traversable aperture. sprite_col has already been bit-7 reflected. */
int avp_gpu_maze2_centered_visibility(unsigned type,u8 open,int sprite_col,
                                       const u8 *window_pairs);

#ifdef __cplusplus
}
#endif
#endif
