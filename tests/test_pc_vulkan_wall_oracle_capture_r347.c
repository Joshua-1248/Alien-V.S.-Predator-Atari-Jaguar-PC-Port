/* RE47 R347: the data compared to GPU readback must remain identical to the
 * submitted source data after all temporary scene/staging owners go away. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pc/pc_vulkan_wall_oracle_capture.h"
static void p32(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(v>>(i*8u));}
static void col(uint8_t *r,uint32_t slot,uint32_t enc,int shade)
{
    p32(r,1u);p32(r+4u,slot);p32(r+8u,1u);
    p32(r+12u,0u);p32(r+16u,16u);
    p32(r+20u,32768u);p32(r+24u,16u);
    p32(r+28u,0u);p32(r+32u,1u);p32(r+36u,16u);
    p32(r+40u,enc);p32(r+44u,(uint32_t)shade);
}
int main(void)
{
    uint8_t wire[32u+2u*48u]={0};
    uint8_t rgb[16u*3u]={0},other_rgb[16u*3u]={0};
    uint16_t cry[16u]={0};
    uint8_t alphas[16u];
    uint32_t *lut=malloc(65536u*sizeof(uint32_t));
    AvpGpuRgbImage first={1,16,rgb,NULL,NULL};
    AvpGpuRgbImage second={1,16,other_rgb,cry,NULL};
    const AvpGpuRgbImage *src[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpVkWallOracleCapture capture={0},empty={0};
    uint8_t rgba[2u*16u*4u],bgra[2u*16u*4u],broken[2u*16u*4u];
    uint32_t mismatches=UINT32_MAX;
    assert(lut);
    p32(wire,AVP_VK_WALL_PROJECTION_MAGIC);
    p32(wire+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    p32(wire+8u,2u);p32(wire+12u,16u);
    p32(wire+16u,48u);p32(wire+20u,32u);p32(wire+24u,2u);
    col(wire+32u,5u,AVP_WORLD_IMAGE_RGB8_ALPHA8,0);
    col(wire+80u,6u,AVP_WORLD_IMAGE_CRY16_ALPHA8,-4);
    assert(avp_vk_wall_projection_validate(wire,sizeof wire));
    for(uint32_t y=0;y<16u;y++){
        rgb[y*3u]=(uint8_t)(0x44u+y);
        rgb[y*3u+1u]=0x66u;
        rgb[y*3u+2u]=0x88u;
        cry[y]=(uint16_t)((0x56u<<8u)|(y*10u));
        other_rgb[y*3u]=other_rgb[y*3u+1u]=other_rgb[y*3u+2u]=0x99u;
        alphas[y]=255u;
    }
    for(uint32_t i=0;i<65536u;i++)
        lut[i]=((i&255u)<<16u)|(((i>>8u)&255u)<<8u)|(i&255u);
    src[5u]=&first;src[6u]=&second;
    assert(avp_vk_wall_oracle_capture_build(wire,sizeof wire,src,lut,
        4096u,8192u,&capture));
    assert(capture.texture_count==2u && capture.texture_bytes==128u);
    assert(capture.sources[5u].words[0]==0xff446688u);
    assert(capture.sources[6u].words[0]==0x00ff5600u);
    assert(!avp_vk_wall_oracle_capture_build(wire,sizeof wire,src,lut,
        4096u,8192u,&capture)); /* never overwrite a live capture */
    /* The source images, LUT, and original geometry are no longer trusted:
     * changes to them must not affect the frame captured for GPU comparison. */
    memset(wire,0,sizeof wire);memset(rgb,0,sizeof rgb);
    memset(cry,0,sizeof cry);memset(lut,0,65536u*sizeof(uint32_t));
    assert(avp_vk_wall_pixel_oracle_make(capture.wire,capture.wire_bytes,
        capture.sources,capture.owned_cry_lut,AVP_VK_WALL_PIXEL_RGBA8,
        rgba,sizeof rgba));
    assert(avp_vk_wall_pixel_oracle_make(capture.wire,capture.wire_bytes,
        capture.sources,capture.owned_cry_lut,AVP_VK_WALL_PIXEL_BGRA8,
        bgra,sizeof bgra));
    for(uint32_t y=0u;y<16u;y++){
        size_t a=((size_t)y*2u)*4u,b=a+4u;
        int intensity=(int)y*10-4;if(intensity<0)intensity=0;
        assert(rgba[a]==0x44u+y && rgba[a+1u]==0x66u && rgba[a+2u]==0x88u);
        assert(rgba[b]==(uint8_t)intensity && rgba[b+1u]==0x56u &&
               rgba[b+2u]==(uint8_t)intensity);
        assert(bgra[a]==rgba[a+2u] && bgra[a+2u]==rgba[a]);
        assert(bgra[b]==rgba[b+2u] && bgra[b+2u]==rgba[b]);
    }
    assert(avp_vk_wall_oracle_capture_compare(&capture,
        AVP_VK_WALL_PIXEL_RGBA8,rgba,sizeof rgba,&mismatches) && mismatches==0u);
    assert(avp_vk_wall_oracle_capture_compare(&capture,
        AVP_VK_WALL_PIXEL_BGRA8,bgra,sizeof bgra,&mismatches) && mismatches==0u);
    memcpy(broken,rgba,sizeof rgba);broken[6u]^=0x01u;
    assert(!avp_vk_wall_oracle_capture_compare(&capture,
        AVP_VK_WALL_PIXEL_RGBA8,broken,sizeof broken,&mismatches) && mismatches==1u);
    assert(!avp_vk_wall_oracle_capture_compare(&capture,
        AVP_VK_WALL_PIXEL_RGBA8,rgba,sizeof rgba-1u,&mismatches) && mismatches==UINT32_MAX);
    assert(!avp_vk_wall_oracle_capture_compare(&capture,
        AVP_VK_WALL_PIXEL_RGBA8,rgba,sizeof rgba+1u,&mismatches) && mismatches==UINT32_MAX);
    assert(!avp_vk_wall_oracle_capture_compare(&empty,
        AVP_VK_WALL_PIXEL_RGBA8,rgba,sizeof rgba,&mismatches) && mismatches==UINT32_MAX);
    avp_vk_wall_oracle_capture_dispose(&capture);
    assert(!capture.wire && !capture.owned_cry_lut && !capture.texture_count);
    avp_vk_wall_oracle_capture_dispose(&capture);
    /* New builds fail cleanly and cannot publish partial captures. */
    p32(wire,AVP_VK_WALL_PROJECTION_MAGIC);
    p32(wire+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    p32(wire+8u,2u);p32(wire+12u,16u);
    p32(wire+16u,48u);p32(wire+20u,32u);p32(wire+24u,2u);
    col(wire+32u,5u,AVP_WORLD_IMAGE_RGB8_ALPHA8,0);
    col(wire+80u,6u,AVP_WORLD_IMAGE_CRY16_ALPHA8,-4);
    assert(!avp_vk_wall_oracle_capture_build(wire,sizeof wire,src,NULL,
        4096u,8192u,&capture));
    assert(!avp_vk_wall_oracle_capture_build(wire,sizeof wire,src,lut,
        4096u,64u,&capture));
    assert(!avp_vk_wall_oracle_capture_build(wire,sizeof wire,src,lut,
        32u,8192u,&capture));
    second.alpha=alphas;alphas[14u]=1u;
    assert(!avp_vk_wall_oracle_capture_build(wire,sizeof wire,src,lut,
        4096u,8192u,&capture));
    assert(!capture.wire && !capture.owned_words[5u] && !capture.owned_words[6u]);
    free(lut);
    puts("R347 source/GPU oracle captured ownership, CRY/RGB pixels, channel formats, mismatch and rollback PASS (CPU)");
    return 0;
}
