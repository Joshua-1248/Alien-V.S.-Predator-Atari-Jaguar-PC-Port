#ifndef AVP_JAGUAR_WORLD_TEXTURE_TRANSFER_H
#define AVP_JAGUAR_WORLD_TEXTURE_TRANSFER_H
/* RE46 R324: CPU-side transfer payload for a future Vulkan R32_UINT image.
 * This is not a rendering backend. No Vulkan SDK and no binary game assets
 * are included. The source is packed from the user's game ROM/art at runtime.
 * Limit uploads explicitly; never construct a 74-texture megastaging atlas. */
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/world_scene_packet.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_WORLD_GPU_CRY_LUT_EDGE 256u
#define AVP_WORLD_GPU_CRY_LUT_WORDS (AVP_WORLD_GPU_CRY_LUT_EDGE*AVP_WORLD_GPU_CRY_LUT_EDGE)
/* Format contract: both textures are VK_FORMAT_R32_UINT, one mip at a time.
 * R32_UINT is an INTEGER format: the shader MUST use usampler2D + texelFetch
 * (not hardware linear filtering). Coordinates x,y are zero-based pixels.
 * All components are little-endian in the transfer byte stream. */
typedef struct AvpWorldGpuStaging {
    uint32_t *words; /* host-native u32; validated LE before exposure to WSI */
    size_t word_capacity;
    size_t byte_count;
    uint32_t width,height,encoding;
    int valid;
} AvpWorldGpuStaging;
void avp_world_gpu_staging_dispose(AvpWorldGpuStaging *s);
void avp_world_gpu_staging_invalidate(AvpWorldGpuStaging *s);
/* max_upload_bytes is a hard per-texture limit, not a soft cache hint.
 * Failed calls always invalidate previous output without freeing reusable
 * storage. Source pointers are not retained. */
int avp_world_gpu_stage_image(AvpWorldGpuStaging *s,
                              const AvpGpuRgbImage *source,
                              size_t max_upload_bytes);
/* Original AVP Jaguar conversion, computed at runtime (no table copied).
 * lut[(cry>>8)*256 + (cry&255)] holds 0x00RRGGBB. 65536 u32 words.
 * Use a second 256x256 R32_UINT image; shade is applied to the CRY Y byte
 * before fetching this LUT. */
int avp_world_gpu_build_cry_lut(uint32_t *lut,size_t word_capacity);
/* Opaque-only integer bilinear oracle for the corresponding GLSL shader.
 * Source is row-major R32_UINT. x/y must be in range; weights 0..1024.
 * CRY shade delta can be negative. Returns 0 on invalid data/alpha.
 * RGB fallback shading uses the software renderer's integer convention. */
int avp_world_gpu_opaque_bilinear(const uint32_t *packed,
    uint32_t width,uint32_t height,AvpWorldImageEncoding encoding,
    const uint32_t *cry_lut,
    uint32_t x0,uint32_t x1,uint32_t y0,uint32_t y1,
    uint32_t wx,uint32_t wy,int32_t shade,uint32_t *out_rgb);
#ifdef __cplusplus
}
#endif
#endif
