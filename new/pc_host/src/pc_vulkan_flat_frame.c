/* RE47/R356: original per-flat source references -> immutable source-backed
 * CPU oracle. The actual Vulkan draw uses the same independent AVFL wire. */
#include "pc/pc_vulkan_flat_frame.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
void avp_vk_flat_frame_dispose(AvpVkFlatFrame *f)
{
    if(!f)return;
    avp_vk_flat_projection_dispose(&f->geometry);
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)free(f->owned_words[i]);
    free(f->cry_lut);
    memset(f,0,sizeof(*f));
}
int avp_vk_flat_frame_build(const AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,AvpVkFlatFrame *out)
{
    AvpVkFlatFrame *f=NULL;
    AvpWorldGpuStaging staging={0};
    uint32_t counts[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    int ok=0;
    if(!scene||!state||!out||out->geometry.data || out->geometry.bytes ||
       out->slots.used_count||out->cry_lut)return 0;
    f=(AvpVkFlatFrame*)calloc(1u,sizeof(*f));
    if(!f)return 0;
    if(!avp_vk_flat_projection_build(scene,&f->geometry) ||
       !avp_vk_flat_projection_validate(f->geometry.data,f->geometry.bytes,scene))goto done;
    const uint8_t *wire=f->geometry.data+AVP_VK_FLAT_HEADER_BYTES;
    uint32_t count=f->geometry.pixels;
    /* Count per-source TEXELS, not source images. All active slots must be
     * valid and consistent with the original game image metadata. */
    for(uint32_t i=0u;i<count;i++,wire+=AVP_VK_FLAT_PIXEL_BYTES){
        uint32_t slot=le32(wire);
        if(slot!=AVP_VK_FLAT_NO_IMAGE){
            if(slot>=AVP_WORLD_SCENE_IMAGE_SLOTS)goto done;
            counts[slot]++;
        }
    }
    for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++){
        const AvpGpuRgbImage *image;
        AvpWorldImageEncoding encoding;
        size_t pixel_count;
        if(!counts[slot])continue;
        image=avp_world_scene_source(state,slot);
        if(!image||!image->rgb||image->width<=0||image->height<=0||
           image->width>8192||image->height>8192||
           scene->image[slot].width!=(uint32_t)image->width||
           scene->image[slot].height!=(uint32_t)image->height||
           scene->image[slot].encoding!=(image->cry16?
              AVP_WORLD_IMAGE_CRY16_ALPHA8:AVP_WORLD_IMAGE_RGB8_ALPHA8))goto done;
        pixel_count=(size_t)image->width*(size_t)image->height;
        /* Alpha compositing requires coverage/depth passes, not an
         * approximation in this first opaque-only GPU milestone. */
        if(image->alpha){
            for(size_t i=0u;i<pixel_count;i++)if(image->alpha[i]!=255u)goto done;
        }
        if(!avp_world_gpu_stage_image(&staging,image,64u*1024u*1024u)||
           staging.byte_count!=pixel_count*sizeof(uint32_t)||
           staging.width!=(uint32_t)image->width||
           staging.height!=(uint32_t)image->height)goto done;
        encoding=(AvpWorldImageEncoding)staging.encoding;
        if(encoding!=scene->image[slot].encoding)goto done;
        f->owned_words[slot]=(uint32_t*)malloc(staging.byte_count);
        if(!f->owned_words[slot])goto done;
        memcpy(f->owned_words[slot],staging.words,staging.byte_count);
        f->pixels[slot]=(AvpVkWallPixelSource){f->owned_words[slot],
             staging.width,staging.height,(uint32_t)encoding};
        f->images[slot]=image;
        f->slots.order[f->slots.used_count++]=slot;
        f->slots.slots[slot]=(AvpVkWallSlot){1u,1u,staging.width,
                                            staging.height,(uint32_t)encoding};
        f->slots.active_columns++;
        avp_world_gpu_staging_invalidate(&staging);
    }
    if(f->slots.used_count!=f->geometry.source_slots ||
       f->slots.active_columns!=f->slots.used_count)goto done;
    f->cry_lut=(uint32_t*)malloc(65536u*sizeof(uint32_t));
    if(!f->cry_lut||!avp_world_gpu_build_cry_lut(f->cry_lut,65536u))goto done;
    *out=*f;
    memset(f,0,sizeof(*f));
    ok=1;
done:
    avp_world_gpu_staging_dispose(&staging);
    avp_vk_flat_frame_dispose(f);
    free(f);
    return ok;
}
int avp_vk_flat_frame_oracle_make(const AvpVkFlatFrame *f,uint32_t format,
    uint8_t *dst,size_t bytes)
{
    uint32_t w,h;
    if(!f||!f->geometry.data||!f->cry_lut||!dst||
       (format!=AVP_VK_WALL_PIXEL_RGBA8&&format!=AVP_VK_WALL_PIXEL_BGRA8)||
       f->geometry.bytes<AVP_VK_FLAT_HEADER_BYTES)return 0;
    w=le32(f->geometry.data+8u);h=le32(f->geometry.data+12u);
    if(!w||!h||(uint64_t)w*h*4u>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES||
       bytes!=(size_t)w*h*4u || f->geometry.pixels!=(uint32_t)((size_t)w*h))return 0;
    uint8_t *temp=(uint8_t*)malloc(bytes);
    if(!temp)return 0;
    for(size_t i=0u;i<bytes;i+=4u){
        temp[i]=temp[i+1u]=temp[i+2u]=0u;temp[i+3u]=255u;
    }
    const uint8_t *wire=f->geometry.data+AVP_VK_FLAT_HEADER_BYTES;
    for(size_t i=0u,n=(size_t)w*h;i<n;i++,wire+=AVP_VK_FLAT_PIXEL_BYTES){
        uint32_t slot=le32(wire);
        if(slot==AVP_VK_FLAT_NO_IMAGE)continue;
        if(slot>=AVP_WORLD_SCENE_IMAGE_SLOTS)goto fail;
        const AvpVkWallPixelSource *src=&f->pixels[slot];
        uint32_t tx=le32(wire+4u),ty=le32(wire+8u);
        int32_t shade=(int32_t)le32(wire+12u);
        if(!src->words||!src->width||!src->height||
           tx>=src->width||ty>=src->height||shade < -255||shade>255)goto fail;
        uint32_t raw=src->words[(size_t)ty*src->width+tx],rgb;
        if(src->encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8){
            int y=(int)(raw&255u)+shade;
            if(((raw>>16u)&255u)!=255u)goto fail;
            if(y<0)y=0;
            if(y>255)y=255;
            rgb=f->cry_lut[(raw&0xff00u)|(unsigned)y]&0xffffffu;
        }else if(src->encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8){
            if(((raw>>24u)&255u)!=255u)goto fail;
            rgb=raw&0xffffffu; /* original host RGB flat bypasses CRY shade */
        }else goto fail;
        temp[i*4u]=(uint8_t)(format==AVP_VK_WALL_PIXEL_BGRA8?rgb:rgb>>16u);
        temp[i*4u+1u]=(uint8_t)(rgb>>8u);
        temp[i*4u+2u]=(uint8_t)(format==AVP_VK_WALL_PIXEL_BGRA8?rgb>>16u:rgb);
    }
    memcpy(dst,temp,bytes);
    free(temp);return 1;
fail:
    free(temp);return 0;
}
int avp_vk_flat_frame_compare_composite(const AvpVkFlatFrame *f,
    const AvpVkWallOracleCapture *walls,uint32_t format,
    const uint8_t *gpu,size_t gpu_bytes,uint32_t *mismatches)
{
    uint8_t *expected=NULL,*wall_pixels=NULL;
    uint32_t w,h,errors=0u;
    int ok=0;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!f||!walls||!walls->wire||!gpu||!mismatches||!f->geometry.data||
       gpu_bytes>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES ||
       !avp_vk_wall_projection_validate(walls->wire,walls->wire_bytes))return 0;
    w=le32(f->geometry.data+8u);h=le32(f->geometry.data+12u);
    if(w!=le32(walls->wire+8u)||h!=le32(walls->wire+12u)||
       (uint64_t)w*h*4u!=gpu_bytes)return 0;
    expected=(uint8_t*)malloc(gpu_bytes);
    wall_pixels=(uint8_t*)malloc(gpu_bytes);
    if(!expected||!wall_pixels)goto done;
    if(!avp_vk_flat_frame_oracle_make(f,format,expected,gpu_bytes)||
       !avp_vk_wall_pixel_oracle_make(walls->wire,walls->wire_bytes,
           walls->sources,walls->owned_cry_lut,format,wall_pixels,gpu_bytes))goto done;
    /* Exactly the opaque wall columns overwrite the previously drawn
     * floor/ceiling. Column top/bottom already passed strict validation. */
    const uint32_t stride=le32(walls->wire+16u);
    for(uint32_t x=0u;x<w;x++){
        const uint8_t *col=walls->wire+32u+(size_t)x*stride;
        if(le32(col)==0u)continue;
        for(uint32_t y=le32(col+12u);y<le32(col+16u);y++){
            size_t at=((size_t)y*w+x)*4u;
            memcpy(expected+at,wall_pixels+at,4u);
        }
    }
    for(size_t i=0u;i<gpu_bytes;i+=4u)
        if(memcmp(gpu+i,expected+i,4u)!=0)errors++;
    *mismatches=errors;
    ok=(errors==0u);
done:
    free(wall_pixels);
    free(expected);
    return ok;
}
