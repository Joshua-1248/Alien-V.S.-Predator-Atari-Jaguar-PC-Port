/* RE47/R356: real source-backed floor/ceiling + opaque wall composition
 * reference. Compiles without Vulkan; independent from completed Software
 * framebuffers. The real GPU compare runs only on the builder's RTX GPU. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "pc/pc_vulkan_flat_frame.h"
#include "jaguar/avp_gpu/state.h"
}
#include "jaguar/avp_gpu/projection.hpp"
extern "C" int avp_gpu_retail_flat_intensity_delta(double d)
{
    return avp::retail_flat_intensity_delta(d);
}
static void put32(uint8_t *p,uint32_t x)
{ for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(x>>(8u*i)); }
int main()
{
    AvpWorldScenePacket scene={};
    assert(avp_world_scene_init(&scene,32u,24u));
    scene.map_width=scene.map_height=8u;
    scene.maze=(uint8_t*)calloc(8u*8u,8u);
    assert(scene.maze);
    scene.maze_capacity=8u*8u*8u;
    for(uint32_t cell=0;cell<64u;cell++){
        scene.maze[cell*8u+4u]=1u;
        scene.maze[cell*8u+5u]=2u;
    }
    for(auto &v:scene.flat_image)v=AVP_WORLD_SCENE_INVALID_SLOT;
    scene.flat_image[1u]=10u;scene.flat_image[2u]=11u;
    scene.image[10u]={16u,16u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    scene.image[11u]={16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    scene.camera_x=scene.camera_y=3.5;
    scene.forward_x=1.0;scene.forward_y=0.0;
    scene.right_x=0.0;scene.right_y=1.0;
    scene.focal=double(scene.width)*0.5;
    for(uint32_t x=0;x<scene.width;x++)scene.columns[x].ready=1u;
    std::vector<uint8_t> cry_rgb(16u*16u*3u,0x50u);
    std::vector<uint16_t> cry16(16u*16u,0x9a80u);
    std::vector<uint8_t> rgb(16u*16u*3u,0x00u);
    for(unsigned i=0;i<256u;i++){
        rgb[i*3u]=0x11u;rgb[i*3u+1u]=0x77u;rgb[i*3u+2u]=0xeeu;
    }
    std::vector<uint8_t> wall_rgb(24u*3u,0u);
    for(unsigned y=0;y<24u;y++){
        wall_rgb[y*3u]=0xffu;wall_rgb[y*3u+1u]=0x30u;
        wall_rgb[y*3u+2u]=0x45u;
    }
    AvpAvpGpuState state={};
    state.panel_tex[10u]={16,16,cry_rgb.data(),cry16.data(),nullptr};
    state.panel_tex[11u]={16,16,rgb.data(),nullptr,nullptr};
    AvpVkFlatFrame flat={};
    assert(avp_vk_flat_frame_build(&scene,&state,&flat));
    assert(flat.geometry.pixels==32u*24u && flat.geometry.source_pixels>0u);
    assert(flat.slots.used_count==2u);
    assert(flat.slots.slots[10u].encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert(flat.slots.slots[11u].encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(!avp_vk_flat_frame_build(&scene,&state,&flat));
    std::vector<uint8_t> flat_rgba(32u*24u*4u),flat_bgra(flat_rgba.size());
    assert(avp_vk_flat_frame_oracle_make(&flat,AVP_VK_WALL_PIXEL_RGBA8,
                                         flat_rgba.data(),flat_rgba.size()));
    assert(avp_vk_flat_frame_oracle_make(&flat,AVP_VK_WALL_PIXEL_BGRA8,
                                         flat_bgra.data(),flat_bgra.size()));
    uint32_t seen_rgb=0u,seen_cry=0u,seen_none=0u;
    for(uint32_t y=0;y<24u;y++)for(uint32_t x=0;x<32u;x++){
        AvpVkFlatTexel texel={};
        assert(avp_vk_flat_projection_get(flat.geometry.data,
                   flat.geometry.bytes,x,y,&texel));
        size_t at=((size_t)y*32u+x)*4u;
        assert(flat_bgra[at]==flat_rgba[at+2u]&&
               flat_bgra[at+1u]==flat_rgba[at+1u]&&
               flat_bgra[at+2u]==flat_rgba[at]);
        if(texel.slot==11u){
            seen_rgb++;
            assert(flat_rgba[at]==0x11u&&flat_rgba[at+1u]==0x77u&&
                   flat_rgba[at+2u]==0xeeu);
        }else if(texel.slot==10u)seen_cry++;
        else if(texel.slot==AVP_VK_FLAT_NO_IMAGE){
            seen_none++;
            assert(flat_rgba[at]==0u&&flat_rgba[at+1u]==0u);
        }else assert(false);
    }
    assert(seen_rgb&&seen_cry&&seen_none);
    /* Build one opaque wall column at x=15, covering y=4..19. The GPU
     * reference must overwrite exactly those pixels, not the full scanline. */
    std::vector<uint8_t> wire(32u+32u*48u,0u);
    put32(wire.data(),AVP_VK_WALL_PROJECTION_MAGIC);
    put32(wire.data()+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    put32(wire.data()+8u,32u);put32(wire.data()+12u,24u);
    put32(wire.data()+16u,48u);put32(wire.data()+20u,32u);
    put32(wire.data()+24u,1u);
    uint8_t *c=wire.data()+32u+15u*48u;
    put32(c,1u);put32(c+4u,5u);put32(c+8u,1u);
    put32(c+12u,4u);put32(c+16u,20u);
    put32(c+20u,32768u);put32(c+24u,24u);
    put32(c+28u,0u);put32(c+32u,1u);put32(c+36u,24u);
    put32(c+40u,AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(avp_vk_wall_projection_validate(wire.data(),wire.size()));
    AvpGpuRgbImage wall={1,24,wall_rgb.data(),nullptr,nullptr};
    const AvpGpuRgbImage *wall_images[AVP_WORLD_SCENE_IMAGE_SLOTS]={};
    wall_images[5u]=&wall;
    AvpVkWallOracleCapture capture={};
    assert(avp_vk_wall_oracle_capture_build(wire.data(),wire.size(),
        wall_images,flat.cry_lut,4096u,8192u,&capture));
    std::vector<uint8_t> gpu=flat_rgba;
    for(unsigned y=4u;y<20u;y++){
        size_t at=((size_t)y*32u+15u)*4u;
        gpu[at]=0xffu;gpu[at+1u]=0x30u;gpu[at+2u]=0x45u;
    }
    uint32_t mismatches=UINT32_MAX;
    assert(avp_vk_flat_frame_compare_composite(&flat,&capture,
        AVP_VK_WALL_PIXEL_RGBA8,gpu.data(),gpu.size(),&mismatches));
    assert(mismatches==0u);
    gpu[0]^=1u;
    assert(!avp_vk_flat_frame_compare_composite(&flat,&capture,
        AVP_VK_WALL_PIXEL_RGBA8,gpu.data(),gpu.size(),&mismatches));
    assert(mismatches==1u);
    assert(!avp_vk_flat_frame_compare_composite(&flat,&capture,
        AVP_VK_WALL_PIXEL_RGBA8,gpu.data(),gpu.size()-4u,&mismatches));
    assert(mismatches==UINT32_MAX);
    std::vector<uint8_t> partial_alpha(16u*16u,255u);
    partial_alpha[0]=128u;
    state.panel_tex[10u].alpha=partial_alpha.data();
    AvpVkFlatFrame rejected={};
    assert(!avp_vk_flat_frame_build(&scene,&state,&rejected));
    assert(rejected.geometry.data==nullptr && rejected.slots.used_count==0u);
    avp_vk_flat_frame_dispose(&flat);
    avp_vk_flat_frame_dispose(&rejected);
    avp_vk_wall_oracle_capture_dispose(&capture);
    avp_world_scene_destroy(&scene);
    std::puts("R356 original Jaguar CRY/RGB flat source capture, opaque composition, RGBA/BGRA, mismatch, transparency gate PASS");
    return 0;
}
