#ifndef AVP_JAGUAR_AVP_GPU_TEXTURE_FILTER_H
#define AVP_JAGUAR_AVP_GPU_TEXTURE_FILTER_H
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/hud_rmw_host_composite.h"
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Presentation only; 0 is the unchanged Jaguar point sampler. */
enum { AVP_TEXFILTER_NEAREST=0, AVP_TEXFILTER_NEAREST_MIP=1, AVP_TEXFILTER_BILINEAR=2 };
enum { AVP_FILTER_HUD=0, AVP_FILTER_ENEMIES=1, AVP_FILTER_WEAPONS=2,
       AVP_FILTER_GEOMETRY=3, AVP_FILTER_PROPS=4, AVP_FILTER_TARGET_COUNT=5 };
#define AVP_FILTER_MIP_MAX 13
/* Level zero BORROWS the original source, all other levels are owned. */
typedef struct AvpFilterMip {
    unsigned width,height;
    uint8_t *rgba; /* premultiplied RGB and linear alpha, 4 bytes/texel */
    uint16_t *cry; /* QCONV conversion of unmatted RGB; for Jaguar fog shading */
} AvpFilterMip;
typedef struct AvpFilterImage {
    const AvpGpuRgbImage *source;
    const void *source_rgb;
    int source_width,source_height;
    unsigned levels; /* includes level zero */
    AvpFilterMip mip[AVP_FILTER_MIP_MAX];
} AvpFilterImage;
typedef struct AvpFilterCache AvpFilterCache;
AvpFilterCache *avp_filter_cache_create(void);
void avp_filter_cache_destroy(AvpFilterCache *cache);
void avp_filter_cache_clear(AvpFilterCache *cache);
/* Called before worker dispatch, never inside parallel pixel loops. */
void avp_filter_cache_prepare(AvpFilterCache *cache,
                             const AvpGpuRgbImage *world,const AvpGpuRgbImage *notice,
                             const AvpGpuRgbImage *original,const AvpGpuRgbImage *original_notice,
                             int use_hires,const unsigned char *qconv,size_t qconv_size);
const AvpFilterImage *avp_filter_cache_lookup(AvpFilterCache *cache,
                                               const AvpGpuRgbImage *source);
/* Read-only lookup for parallel wall workers. Mip chains must be preheated
 * by the render owner before worker dispatch; never allocate from workers. */
const AvpFilterImage *avp_filter_cache_lookup_ready(const AvpFilterCache *cache,
                                               const AvpGpuRgbImage *source);
void avp_filter_image_destroy(AvpFilterImage *image);
int avp_filter_image_build(AvpFilterImage *out,const AvpGpuRgbImage *source,
                           const unsigned char *qconv,size_t qconv_size);
/* R390: same exact mip calculations; retains appropriately sized allocations
 * between viewport sprite draws. Caller owns and eventually destroys chain. */
int avp_filter_image_rebuild(AvpFilterImage *out,const AvpGpuRgbImage *source,
                           const unsigned char *qconv,size_t qconv_size);
/* UV in source-image units [0..1].  Derivatives are output-pixel UV deltas. */
typedef struct AvpFilterDerivatives {double dudx,dvdx,dudy,dvdy;} AvpFilterDerivatives;
/* Precalculate the footprint once when the derivatives and source do not
 * change (particularly the many pixels in a projected wall column). */
typedef struct AvpFilterPlan {
    unsigned level,taps,bilinear;
    double du,dv;
} AvpFilterPlan;
AvpFilterPlan avp_filter_plan(const AvpGpuRgbImage *source,
                              const AvpFilterImage *mips,AvpFilterDerivatives dd,
                              unsigned mode,unsigned aniso);
uint32_t avp_filter_sample_prepared(const AvpGpuRgbImage *source,
                            const AvpFilterImage *mips,double u,double v,
                            AvpFilterPlan plan,int wrap,int shade,
                            const uint32_t *cry_rgb,unsigned *out_alpha);
/* Host floor/ceiling only: the shade is invariant across a row. */
void avp_filter_make_shade_lut(uint8_t lut[256],int shade);
uint32_t avp_filter_sample_prepared_shaded(const AvpGpuRgbImage *source,
                            const AvpFilterImage *mips,double u,double v,
                            AvpFilterPlan plan,int wrap,int shade,
                            const uint32_t *cry_rgb,unsigned *out_alpha,
                            const uint8_t shade_lut[256]);
/* R313: the texture U coordinate, U-axis bilinear texels and U weights
 * are constant down a projected wall column. Prepare once on the caller's
 * render worker stack and reuse them across all output rows and AF taps.
 * Only opaque CRY16 + clamped + bilinear falls into this exact fast path;
 * every other format keeps avp_filter_sample_prepared_shaded(). */
typedef struct AvpFilterCryWallColumn {
    const uint16_t *cry;
    const uint32_t *cry_rgb;
    const uint8_t *shade_lut;
    unsigned width,height,taps;
    unsigned x0[16],x1[16],ax[16];
    double v_offset[16];
} AvpFilterCryWallColumn;
int avp_filter_cry_wall_column_prepare(AvpFilterCryWallColumn *column,
    const AvpGpuRgbImage *source,const AvpFilterImage *mips,
    double u,AvpFilterPlan plan,int wrap,
    const uint32_t *cry_rgb,const uint8_t shade_lut[256]);
uint32_t avp_filter_cry_wall_column_sample(
    const AvpFilterCryWallColumn *column,double v);
/* R314: on a floor/ceiling scanline, a plan and source mip remain fixed
 * across nearby pixels. Bind opaque CRY16 input and exact
 * anisotropic offsets once per plan block, then vary only the per-pixel UV.
 * Other image formats, transparency and non-bilinear modes fall back. */
typedef struct AvpFilterCryFloorBlock {
    const uint16_t *cry;
    const uint32_t *cry_rgb;
    const uint8_t *shade_lut;
    unsigned width,height,taps,power_of_two;
    double u_offset[16],v_offset[16];
} AvpFilterCryFloorBlock;
int avp_filter_cry_floor_block_prepare(AvpFilterCryFloorBlock *block,
    const AvpGpuRgbImage *source,const AvpFilterImage *mips,
    AvpFilterPlan plan,int wrap,const uint32_t *cry_rgb,
    const uint8_t shade_lut[256]);
uint32_t avp_filter_cry_floor_block_sample(
    const AvpFilterCryFloorBlock *block,double u,double v);
uint32_t avp_filter_sample(const AvpGpuRgbImage *source,const AvpFilterImage *mips,
                            double u,double v,AvpFilterDerivatives dd,
                            unsigned mode,unsigned aniso,int wrap,int shade,
                            const uint32_t *cry_rgb,unsigned *out_alpha);
/* Host native-overlay alpha-aware bilinear for the Jaguar 320x228 sprites. */
void avp_filter_native_overlay(uint32_t *host,unsigned w,unsigned h,
                               const uint32_t *before,const uint32_t *after);
void avp_filter_hud_replay(uint32_t *host,unsigned w,unsigned h,
                           const uint32_t *before,const uint32_t *after,
                           const AvpHudRmwOps *ops,AvpHudRmwApply apply,void *context,
                           uint32_t *background_mask,unsigned background_stride);
#ifdef __cplusplus
}
#endif
#endif
