/* RE46 R324 — Vulkan integer-image staging contract and CPU parity oracle.
 * All CRY color conversion delegates to the port's existing Jaguar layer.
 * No Vulkan dependency: SDK/driver failure cannot affect Software startup. */
#include "jaguar/avp_gpu/world_texture_transfer.h"
#include "jaguar/cry.h"
#include <stdlib.h>
#include <string.h>

void avp_world_gpu_staging_dispose(AvpWorldGpuStaging *s)
{
    if (!s) return;
    free(s->words);
    memset(s,0,sizeof(*s));
}
void avp_world_gpu_staging_invalidate(AvpWorldGpuStaging *s)
{
    if (!s) return;
    s->valid=0;
    s->byte_count=0;
    s->width=s->height=s->encoding=0;
}
static int avp_world_gpu_little_endian(void)
{
    const uint16_t one=1u;
    return *(const uint8_t *)&one==1u;
}
int avp_world_gpu_stage_image(AvpWorldGpuStaging *s,
                              const AvpGpuRgbImage *source,
                              size_t max_upload_bytes)
{
    size_t count,bytes;
    AvpWorldImageEncoding encoding=AVP_WORLD_IMAGE_ABSENT;
    uint32_t *replacement;
    if (!s) return 0;
    avp_world_gpu_staging_invalidate(s);
    /* R32_UINT requires four-byte words. Native LE is checked because
     * Vulkan uploads are a byte stream, never C struct serialization. */
    if (!source || !source->rgb || !source->width || !source->height ||
        source->width>8192 || source->height>8192 ||
        !avp_world_gpu_little_endian() ||
        (size_t)source->width>SIZE_MAX/(size_t)source->height/4u)
        return 0;
    count=(size_t)source->width*(size_t)source->height;
    bytes=count*sizeof(uint32_t);
    if (!max_upload_bytes || bytes>max_upload_bytes)return 0;
    if (count>s->word_capacity) {
        replacement=realloc(s->words,bytes);
        if (!replacement) return 0;
        s->words=replacement;
        s->word_capacity=count;
    }
    if (!avp_world_scene_pack_image(source,s->words,s->word_capacity,&encoding))
        return 0;
    if (encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
        encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8)return 0;
    s->byte_count=bytes;
    s->width=(uint32_t)source->width;
    s->height=(uint32_t)source->height;
    s->encoding=(uint32_t)encoding;
    s->valid=1;
    return 1;
}
int avp_world_gpu_build_cry_lut(uint32_t *lut,size_t capacity)
{
    if (!lut || capacity<AVP_WORLD_GPU_CRY_LUT_WORDS ||
        !avp_world_gpu_little_endian())return 0;
    for (uint32_t cry=0;cry<AVP_WORLD_GPU_CRY_LUT_WORDS;cry++) {
        uint8_t r=0,g=0,b=0;
        avp_jaguar_cry16_to_rgb((uint16_t)cry,&r,&g,&b);
        lut[cry]=((uint32_t)r<<16u)|((uint32_t)g<<8u)|(uint32_t)b;
    }
    return 1;
}
static unsigned avp_gpu_channel(uint32_t texel,unsigned shift)
{
    return (texel>>shift)&255u;
}
static unsigned avp_gpu_texel_rgb(uint32_t texel,AvpWorldImageEncoding encoding,
                                  const uint32_t *lut,int32_t shade,uint32_t *rgb)
{
    unsigned alpha;
    if (encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8) {
        uint32_t cry=texel&65535u;
        int64_t y=(int64_t)(cry&255u)+(int64_t)shade;
        alpha=(texel>>16u)&255u;
        if (y<0)y=0;
        if (y>255)y=255;
        *rgb=lut[(cry&65280u)|(uint32_t)y];
    } else {
        uint32_t r,g,b,k;
        alpha=(texel>>24u)&255u;
        r=avp_gpu_channel(texel,16u);
        g=avp_gpu_channel(texel,8u);
        b=avp_gpu_channel(texel,0u);
        if (shade<0) {
            k=(shade< -255)?0u:(uint32_t)(255+shade);
            r=r*k/255u;g=g*k/255u;b=b*k/255u;
        }
        *rgb=(r<<16u)|(g<<8u)|b;
    }
    return alpha;
}
int avp_world_gpu_opaque_bilinear(const uint32_t *packed,
    uint32_t width,uint32_t height,AvpWorldImageEncoding encoding,
    const uint32_t *lut,uint32_t x0,uint32_t x1,uint32_t y0,uint32_t y1,
    uint32_t wx,uint32_t wy,int32_t shade,uint32_t *out_rgb)
{
    uint32_t c[4],w[4],r=0,g=0,b=0;
    size_t at[4];
    if (!packed || !out_rgb || !width || !height || !lut ||
        (encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
         encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8) ||
        x0>=width || x1>=width || y0>=height || y1>=height ||
        wx>1024u || wy>1024u || (size_t)width>SIZE_MAX/(size_t)height)
        return 0;
    at[0]=(size_t)y0*width+x0;
    at[1]=(size_t)y0*width+x1;
    at[2]=(size_t)y1*width+x0;
    at[3]=(size_t)y1*width+x1;
    for (unsigned i=0;i<4u;i++){
        if (avp_gpu_texel_rgb(packed[at[i]],encoding,lut,shade,&c[i])!=255u)
            return 0; /* never silently treat transparent art as opaque */
    }
    w[0]=(1024u-wx)*(1024u-wy);
    w[1]=wx*(1024u-wy);
    w[2]=(1024u-wx)*wy;
    w[3]=wx*wy;
    for (unsigned i=0;i<4u;i++){
        r+=avp_gpu_channel(c[i],16u)*w[i];
        g+=avp_gpu_channel(c[i],8u)*w[i];
        b+=avp_gpu_channel(c[i],0u)*w[i];
    }
    *out_rgb=(((r+524288u)>>20u)<<16u)|
             (((g+524288u)>>20u)<<8u)|((b+524288u)>>20u);
    return 1;
}
