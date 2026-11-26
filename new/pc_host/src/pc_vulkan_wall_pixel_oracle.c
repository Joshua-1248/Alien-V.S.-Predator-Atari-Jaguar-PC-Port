/* R346: source-texture wall pixel oracle. This deliberately runs without
 * Vulkan headers or a GPU; GPU correctness still requires a real readback. */
#include "pc/pc_vulkan_wall_pixel_oracle.h"
#include <stdlib.h>
#include <string.h>

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static uint32_t exact_row(const uint8_t *wire,const uint8_t *r,
                          uint32_t version,uint32_t y)
{
    const uint32_t top=read32(r+12u),th=read32(r+36u);
    if(version==AVP_VK_WALL_PROJECTION_EXACT_VERSION){
        uint32_t offset=read32(r+48u);
        if(offset){
            uint32_t pixel=y-top;
            uint32_t word=read32(wire+4u*(size_t)(offset+pixel/2u));
            return (word>>((pixel&1u)*16u))&65535u;
        }
    }
    /* Match the shader's clamped, truncating integer source-row path.
     * 64-bit intermediates avoid undefined overflow on the host. */
    const int64_t base=(int64_t)read32(r+20u)-32768;
    const uint32_t projected=read32(r+24u);
    const int64_t n=((int64_t)y-base)*2+1;
    int64_t row;
    if(th>128u)row=n*(int64_t)th/(2*(int64_t)projected);
    else {
        int64_t native=n*128/(2*(int64_t)projected);
        if(native<0)native=0;
        if(native>127)native=127;
        row=native*(int64_t)th/128;
    }
    if(row<0)row=0;
    if(row>=(int64_t)th)row=(int64_t)th-1;
    return (uint32_t)row;
}
static int source_rgb(uint32_t raw,uint32_t encoding,int32_t shade,
                      const uint32_t *lut,uint32_t *rgb)
{
    if(encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8){
        int64_t y=(int64_t)(raw&255u)+shade;
        if(((raw>>16u)&255u)!=255u || !lut)return 0;
        if(y<0)y=0;
        if(y>255)y=255;
        *rgb=lut[(raw&0xff00u)|(uint32_t)y]&0xffffffu;
    }else if(encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8){
        if(((raw>>24u)&255u)!=255u || shade!=0)return 0;
        *rgb=raw&0xffffffu;
    }else return 0;
    return 1;
}
int avp_vk_wall_pixel_oracle_make(const uint8_t *wire,size_t wire_bytes,
    const AvpVkWallPixelSource sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    const uint32_t *cry_lut,uint32_t format,uint8_t *dst,size_t dst_bytes)
{
    uint8_t *temp=NULL;
    uint32_t width,height,stride,version;
    size_t bytes;
    int ok=0;
    if(!sources || !dst || (format!=AVP_VK_WALL_PIXEL_RGBA8 &&
       format!=AVP_VK_WALL_PIXEL_BGRA8) ||
       !avp_vk_wall_projection_validate(wire,wire_bytes))return 0;
    width=read32(wire+8u);height=read32(wire+12u);
    stride=read32(wire+16u);version=read32(wire+4u);
    if((uint64_t)width*height*4u>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES)
        return 0;
    bytes=(size_t)width*height*4u;
    if(dst_bytes<bytes)return 0;
    /* Check all active image bindings before writing any user memory. */
    for(uint32_t x=0;x<width;x++){
        const uint8_t *r=wire+32u+(size_t)x*stride;
        if(read32(r)){
            uint32_t slot=read32(r+4u),encoding=read32(r+40u);
            const AvpVkWallPixelSource *s=&sources[slot];
            if(!s->words || s->width!=read32(r+32u) ||
               s->height!=read32(r+36u) || s->encoding!=encoding ||
               (encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8 && !cry_lut))
                return 0;
        }
    }
    temp=malloc(bytes);
    if(!temp)return 0;
    for(size_t i=0;i<bytes;i+=4u){
        temp[i]=temp[i+1u]=temp[i+2u]=0u;temp[i+3u]=255u;
    }
    for(uint32_t x=0;x<width;x++){
        const uint8_t *r=wire+32u+(size_t)x*stride;
        if(!read32(r))continue;
        const uint32_t top=read32(r+12u),bottom=read32(r+16u);
        const uint32_t slot=read32(r+4u),encoding=read32(r+40u);
        const AvpVkWallPixelSource *s=&sources[slot];
        const uint32_t source_x=read32(r+28u);
        const int32_t shade=version>=AVP_VK_WALL_PROJECTION_LIT_VERSION ?
            (int32_t)read32(r+44u) : 0;
        for(uint32_t y=top;y<bottom;y++){
            const uint32_t source_y=exact_row(wire,r,version,y);
            uint32_t rgb;
            size_t at;
            if(source_y>=s->height ||
               !source_rgb(s->words[(size_t)source_y*s->width+source_x],
                           encoding,shade,cry_lut,&rgb))goto done;
            at=((size_t)y*width+x)*4u;
            temp[at]=(uint8_t)(format==AVP_VK_WALL_PIXEL_BGRA8?rgb:rgb>>16u);
            temp[at+1u]=(uint8_t)(rgb>>8u);
            temp[at+2u]=(uint8_t)(format==AVP_VK_WALL_PIXEL_BGRA8?rgb>>16u:rgb);
        }
    }
    memcpy(dst,temp,bytes);
    ok=1;
done:
    free(temp);
    return ok;
}
int avp_vk_wall_pixel_oracle_compare(const uint8_t *wire,size_t wire_bytes,
    const AvpVkWallPixelSource sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    const uint32_t *cry_lut,uint32_t format,
    const uint8_t *gpu_pixels,size_t gpu_bytes,uint32_t *mismatches)
{
    uint8_t *expected=NULL;
    size_t bytes;
    uint32_t errors=0u;
    if(!mismatches)return 0;
    *mismatches=UINT32_MAX;
    if(!gpu_pixels || !avp_vk_wall_projection_validate(wire,wire_bytes))return 0;
    if((uint64_t)read32(wire+8u)*read32(wire+12u)*4u>
       AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES)return 0;
    bytes=(size_t)read32(wire+8u)*read32(wire+12u)*4u;
    if(gpu_bytes<bytes)return 0;
    expected=malloc(bytes);
    if(!expected)return 0;
    if(!avp_vk_wall_pixel_oracle_make(wire,wire_bytes,sources,cry_lut,
                                      format,expected,bytes)){
        free(expected);return 0;
    }
    /* One pixel mismatch even if multiple channels differ. */
    for(size_t i=0;i<bytes;i+=4u)
        if(memcmp(gpu_pixels+i,expected+i,4u)!=0)++errors;
    free(expected);
    *mismatches=errors;
    return errors==0u;
}
