#ifndef AVP_JAGUAR_AVP_GPU_BRIDGE_H
#define AVP_JAGUAR_AVP_GPU_BRIDGE_H
#include "avp_types.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AvpGpuRgbImage { int width,height; u8 *rgb; u16 *cry16; } AvpGpuRgbImage;
int avp_gpu_decode_jagpeg(const u8 *data,size_t size,int quality,int corrected_chroma,AvpGpuRgbImage *out);
void avp_gpu_free_rgb_image(AvpGpuRgbImage *img);
int avp_gpu_build_cry_image(AvpGpuRgbImage *img,const u8 *qconv,size_t qconv_size);
int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file_id);
int avp_gpu_wall_notice_index(unsigned level,u8 wall,unsigned *notice_index);
int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file_id);
unsigned avp_gpu_flat_texture_rotation(unsigned level,u8 flat);
int avp_gpu_wall_window_pair_file(unsigned level,u8 wall,unsigned *file_id);
int avp_gpu_door_visual_column_open(u8 wall,u8 open,int panel_pos);
unsigned avp_gpu_door_opening_limit(u8 wall);
int avp_gpu_retail_wall_intensity_delta(double distance_cells);
int avp_gpu_retail_flat_intensity_delta(double distance_cells);
#ifdef __cplusplus
}
#endif
#endif
