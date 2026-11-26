#ifndef AVP_PC_VULKAN_RAY_COLUMNS_H
#define AVP_PC_VULKAN_RAY_COLUMNS_H
/* R335 independent synthetic geometry diagnostic. No ROM, source images, or
 * completed software framebuffer. Wire format matches R331 exactly. */
#include <stddef.h>
#include <stdint.h>
/* Returns the exact number of bytes needed, or zero for invalid dimensions. */
size_t avp_vk_diag_ray_columns_size(uint32_t width,uint32_t height);
/* Fill caller-owned storage; reject short buffers and invalid dimensions. */
int avp_vk_diag_ray_columns_make(uint8_t *data,size_t capacity,
                                 uint32_t width,uint32_t height);
/* Shader output oracle for a 16x16 source checkerboard. */
uint32_t avp_vk_diag_ray_columns_color(uint32_t x,uint32_t y);
#endif
