/* R335: SDK-free parity and corruption gates for full-width ray-buffer shader. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_ray_columns.h"
#include "pc/pc_vulkan_geometry_policy.h"
#include "pc/pc_vulkan_readback_policy.h"
#include "jaguar/avp_gpu/world_scene_gpu_wire.h"

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8u) |
           ((uint32_t)p[2]<<16u) | ((uint32_t)p[3]<<24u);
}
static void test_dimensions(uint32_t w,uint32_t h)
{
    const size_t bytes=avp_vk_diag_ray_columns_size(w,h);
    uint8_t *data=malloc(bytes);
    assert(bytes && data);
    assert(!avp_vk_diag_ray_columns_make(data,bytes-1u,w,h));
    assert(avp_vk_diag_ray_columns_make(data,bytes,w,h));
    assert(avp_vk_geometry_wire_valid(data,bytes));
    assert(bytes == 32u+(size_t)64u*w);
    assert(read32(data+8u)==w && read32(data+12u)==h);
    assert(read32(data+16u)==w);
    assert(read32(data+24u)==32u+w*24u);
    for(uint32_t x=0u;x<w;++x){
        const uint8_t *c=data+32u+(size_t)x*24u;
        const uint8_t *hit=data+32u+(size_t)w*24u+(size_t)x*40u;
        assert(read32(c)==x && read32(c+4u)==1u);
        assert(read32(c+16u)==1u && read32(c+20u)==0u);
        assert(read32(hit+16u)==1u+((x/8u)&1u));
        assert(read32(hit+28u)==1u);
        assert(read32(hit+32u)==0u && read32(hit+36u)==h);
    }
    /* Corrupt a middle column: validator must catch missing readiness. */
    data[32u+(size_t)(w/2u)*24u+16u]=0u;
    assert(!avp_vk_geometry_wire_valid(data,bytes));
    data[32u+(size_t)(w/2u)*24u+16u]=1u;
    assert(avp_vk_geometry_wire_valid(data,bytes));
    /* Corrupt hit count/order to ensure upload would be rejected. */
    data[32u+4u]=2u;
    assert(!avp_vk_geometry_wire_valid(data,bytes));
    free(data);
}
static void test_pixels(uint32_t w,uint32_t h,uint32_t format)
{
    size_t bytes=0u;
    uint8_t *image;
    uint32_t errors=UINT32_MAX;
    assert(avp_vk_readback_spec(w,h,format,&bytes));
    image=malloc(bytes);assert(image);
    for(uint32_t y=0u;y<h;++y)for(uint32_t x=0u;x<w;++x){
        uint32_t rgb=avp_vk_diag_ray_columns_color(x,y);
        size_t i=((size_t)y*w+x)*4u;
        image[i]= (uint8_t)(rgb>>(format==AVP_VK_READBACK_BGRA8_UNORM?0u:16u));
        image[i+1u]=(uint8_t)(rgb>>8u);
        image[i+2u]=(uint8_t)(rgb>>(format==AVP_VK_READBACK_BGRA8_UNORM?16u:0u));
        image[i+3u]=255u;
    }
    assert(avp_vk_readback_check_ray_columns(image,bytes,w,h,format,&errors));
    assert(errors==0u);
    /* Existing R330 checkerboard oracle must NOT accept geometry-shifted image. */
    assert(!avp_vk_readback_check(image,bytes,w,h,format,&errors));
    assert(errors>0u);
    /* A single pixel must be detected exactly. */
    image[((size_t)(h/2u)*w+w/2u)*4u]^=1u;
    assert(!avp_vk_readback_check_ray_columns(image,bytes,w,h,format,&errors));
    assert(errors==1u);
    assert(!avp_vk_readback_check_ray_columns(image,bytes-1u,w,h,format,&errors));
    free(image);
}
int main(void)
{
    assert(avp_vk_diag_ray_columns_size(0u,10u)==0u);
    assert(avp_vk_diag_ray_columns_size(10u,0u)==0u);
    assert(avp_vk_diag_ray_columns_size(8193u,10u)==0u);
    assert(avp_vk_diag_ray_columns_size(10u,8193u)==0u);
    assert(!avp_vk_diag_ray_columns_make(NULL,0u,640u,480u));
    test_dimensions(1u,1u);
    test_dimensions(319u,257u);
    test_dimensions(640u,480u);
    test_dimensions(1920u,1080u);
    test_dimensions(8192u,8192u);
    test_pixels(319u,257u,AVP_VK_READBACK_BGRA8_UNORM);
    test_pixels(319u,257u,AVP_VK_READBACK_RGBA8_UNORM);
    test_pixels(640u,480u,AVP_VK_READBACK_BGRA8_UNORM);
    test_pixels(640u,480u,AVP_VK_READBACK_RGBA8_UNORM);
    puts("R335 synthetic ray columns: 5 wire extents and 4 framebuffer parity cases PASS");
    return 0;
}
