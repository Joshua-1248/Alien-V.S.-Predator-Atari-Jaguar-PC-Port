/* R358: authentic source-texel GPU tape for window/door/portal plus floors.
 * Covers binary transparent window pixels, overlap order, bit-exact CRY/RGB,
 * transactional failure, visible source validation, RGBA/BGRA and mismatch. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>
extern "C" {
#include "pc/pc_vulkan_layered_frame.h"
#include "jaguar/avp_gpu/state.h"
}
extern "C" int avp_gpu_retail_wall_intensity_delta(double) {return -12;}
extern "C" int avp_gpu_retail_flat_intensity_delta(double) {return -30;}
int main()
{
    AvpWorldScenePacket scene={};
    assert(avp_world_scene_init(&scene,32u,64u));
    scene.map_width=scene.map_height=8u;
    scene.maze=(uint8_t*)calloc(64u,8u);
    assert(scene.maze);
    scene.maze_capacity=512u;
    for(unsigned i=0;i<64u;i++){
        scene.maze[i*8u+4u]=1u;
        scene.maze[i*8u+5u]=2u;
    }
    for(auto &v:scene.flat_image)v=AVP_WORLD_SCENE_INVALID_SLOT;
    scene.flat_image[1u]=12u;scene.flat_image[2u]=12u;
    for(auto &v:scene.wall_image)v=10u;
    scene.wall_image[3u<<3u]=11u;
    scene.image[10u]={128u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    scene.image[11u]={128u,128u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    scene.image[12u]={16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    scene.window_pair_loaded[0]=1u;
    scene.wall_window_pair[3u<<3u]=0u;
    for(unsigned i=0;i<128u;i++){
        scene.window_pairs[0][i*2u]=(uint8_t)-18;
        scene.window_pairs[0][i*2u+1u]=(uint8_t)18;
    }
    scene.camera_x=scene.camera_y=3.5;
    scene.forward_x=1.0;scene.right_y=1.0;
    scene.focal=16.0;
    for(uint32_t x=0;x<scene.width;x++){
        /* Front window frame, then back wall seen through its aperture. */
        if((x%2u)==0u)
            assert(avp_world_scene_hit(&scene,x,1.0,.50,3u<<3u,
                    0u,0u,0u,0,64));
        assert(avp_world_scene_hit(&scene,x,2.0,.25,1u,
                0u,0u,1u,0,64));
        avp_world_scene_end_column(&scene,x,2.0);
    }
    assert(avp_world_scene_complete(&scene));
    std::vector<uint8_t> cry_rgb(128u*128u*3u,0x66);
    std::vector<uint16_t> cry(128u*128u,0x9a80u);
    std::vector<uint8_t> window_rgb(128u*128u*3u,0x00u);
    std::vector<uint8_t> window_alpha(128u*128u,255u);
    for(unsigned i=0;i<128u*128u;i++){
        window_rgb[i*3u+0u]=0x81u;
        window_rgb[i*3u+1u]=0x27u;
        window_rgb[i*3u+2u]=0x39u;
    }
    /* Exact visible front texture fraction is source x=64.  Its upper
     * pixel rows are fully masked; remaining pixel rows remain opaque. */
    for(unsigned y=0;y<64u;y++)window_alpha[y*128u+64u]=0u;
    std::vector<uint8_t> flat_rgb(16u*16u*3u,0x20u);
    AvpAvpGpuState state={};
    state.panel_tex[10u]={128,128,cry_rgb.data(),cry.data(),nullptr};
    state.panel_tex[11u]={128,128,window_rgb.data(),nullptr,window_alpha.data()};
    state.panel_tex[12u]={16,16,flat_rgb.data(),nullptr,nullptr};
    AvpVkLayeredFrame f={};
    assert(avp_vk_layered_frame_build(&scene,&state,&f));
    assert(f.layers.portal_columns==16u);
    assert(f.layers.window_columns>=16u);
    assert(f.masked_pixels>0u&&f.wall_coverage_pixels>0u);
    assert(f.slots.used_count>=2u);
    assert(avp_vk_layered_frame_validate(&f));
    assert(avp_vk_layered_frame_ready_for_present(&f));
    assert(!avp_vk_layered_frame_build(&scene,&state,&f));
    /* R375: per-texture scissor may never crop a sampled destination pixel.
     * Exercise odd upscales, downscales and window-letterbox offsets. */
    for(const auto &dims: std::vector<std::pair<uint32_t,uint32_t>>{
            {32u,64u},{101u,153u},{7u,11u},{1920u,1080u}}){
        for(uint32_t j=0;j<f.slots.used_count;j++){
            const uint32_t slot=f.slots.order[j];
            uint32_t xx=0u,yy=0u,ww=0u,hh=0u;
            assert(avp_vk_layered_frame_slot_scissor(&f,slot,13u,7u,
                dims.first,dims.second,&xx,&yy,&ww,&hh));
            assert(xx>=13u&&yy>=7u&&xx+ww<=13u+dims.first&&
                   yy+hh<=7u+dims.second);
            for(uint32_t dy=0;dy<dims.second;dy++){
                uint32_t sy=(uint64_t)dy*64u/dims.second;
                for(uint32_t dx=0;dx<dims.first;dx++){
                    uint32_t sx=(uint64_t)dx*32u/dims.first;
                    const uint8_t *ptr=f.geometry.data+AVP_VK_FLAT_HEADER_BYTES+
                        ((size_t)sy*32u+sx)*16u;
                    uint32_t native_slot=(uint32_t)ptr[0]|((uint32_t)ptr[1]<<8u)|
                        ((uint32_t)ptr[2]<<16u)|((uint32_t)ptr[3]<<24u);
                    if(native_slot==slot)
                        assert(dx+13u>=xx&&dx+13u<xx+ww&&
                               dy+7u>=yy&&dy+7u<yy+hh);
                }
            }
        }
    }
    assert(f.upload_images[11u].alpha==nullptr); // sanitized for resolved GPU
    assert(f.images[11u]->alpha==window_alpha.data()); // original retained
    std::vector<uint8_t> rgba(32u*64u*4u),bgra(rgba.size());
    assert(avp_vk_layered_frame_oracle_make(&f,AVP_VK_WALL_PIXEL_RGBA8,
                                             rgba.data(),rgba.size()));
    assert(avp_vk_layered_frame_oracle_make(&f,AVP_VK_WALL_PIXEL_BGRA8,
                                             bgra.data(),bgra.size()));
    for(size_t i=0;i<rgba.size();i+=4u){
        assert(rgba[i]==bgra[i+2u]&&rgba[i+1u]==bgra[i+1u]&&
               rgba[i+2u]==bgra[i]&&rgba[i+3u]==255u);
    }
    /* R374: direct full Software/world pixel oracle must agree with the
     * original independent RGBA construction, including masked windows. */
    std::vector<uint32_t> software_world(32u*64u);
    for(size_t i=0;i<software_world.size();i++)
        software_world[i]=((uint32_t)rgba[i*4u]<<16u)|
                          ((uint32_t)rgba[i*4u+1u]<<8u)|rgba[i*4u+2u];
    uint32_t world_mismatches=UINT32_MAX;
    assert(avp_vk_layered_frame_oracle_matches_world(&f,software_world.data(),
            32u,32u,64u,&world_mismatches)&&world_mismatches==0u);
    assert(avp_vk_layered_frame_validated_tape_matches_world(&f,
            software_world.data(),32u,32u,64u,&world_mismatches)&&world_mismatches==0u);
    software_world[42u]^=0x10u;
    assert(!avp_vk_layered_frame_oracle_matches_world(&f,software_world.data(),
            32u,32u,64u,&world_mismatches)&&world_mismatches==1u);
    assert(!avp_vk_layered_frame_validated_tape_matches_world(&f,
            software_world.data(),32u,32u,64u,&world_mismatches)&&world_mismatches==1u);
    software_world[42u]^=0x10u;
    assert(!avp_vk_layered_frame_oracle_matches_world(&f,software_world.data(),
            32u,32u,65u,&world_mismatches)&&world_mismatches==UINT32_MAX);
    uint32_t mismatches=UINT32_MAX;
    assert(avp_vk_layered_frame_compare(&f,AVP_VK_WALL_PIXEL_RGBA8,
                                         rgba.data(),rgba.size(),&mismatches));
    assert(mismatches==0u);
    rgba[12u]^=1u;
    assert(!avp_vk_layered_frame_compare(&f,AVP_VK_WALL_PIXEL_RGBA8,
                                          rgba.data(),rgba.size(),&mismatches));
    assert(mismatches==1u);
    assert(!avp_vk_layered_frame_compare(&f,AVP_VK_WALL_PIXEL_RGBA8,
                                          rgba.data(),rgba.size()-4u,&mismatches));
    assert(mismatches==UINT32_MAX);
    /* Source-coordinate tape corruption cannot silently pass validation. */
    uint8_t *record=f.geometry.data+AVP_VK_FLAT_HEADER_BYTES;
    uint8_t old=record[0];record[0]^=1u;
    assert(!avp_vk_layered_frame_validate(&f));
    record[0]=old;
    assert(avp_vk_layered_frame_validate(&f));
    /* R376: same original scene, exact byte-for-byte single-thread and
     * independently bounded four-worker original-source composition. */
    {
        AvpVkLayeredFrame parallel={};
        assert(setenv("AVP_HYBRID_SOURCE_THREADS","4",1)==0);
        assert(avp_vk_layered_frame_build(&scene,&state,&parallel));
        assert(parallel.geometry.bytes==f.geometry.bytes);
        assert(memcmp(parallel.geometry.data,f.geometry.data,f.geometry.bytes)==0);
        assert(parallel.masked_pixels==f.masked_pixels);
        assert(parallel.wall_coverage_pixels==f.wall_coverage_pixels);
        assert(parallel.geometry.source_pixels==f.geometry.source_pixels);
        assert(parallel.slots.used_count==f.slots.used_count);
        assert(memcmp(parallel.visible_bounds,f.visible_bounds,
                      sizeof(f.visible_bounds))==0);
        assert(avp_vk_layered_frame_validate(&parallel));
        avp_vk_layered_frame_dispose(&parallel);
        unsetenv("AVP_HYBRID_SOURCE_THREADS");
    }
    /* R378: column/tile resolver must be BIT-FOR-BIT identical to original
     * per-pixel far-to-near replay, for masked windows and portal overlaps. */
    {
        AvpVkLayeredFrame replay={};
        assert(setenv("AVP_HYBRID_SOURCE_REPLAY","1",1)==0);
        assert(avp_vk_layered_frame_build(&scene,&state,&replay));
        unsetenv("AVP_HYBRID_SOURCE_REPLAY");
        assert(replay.geometry.bytes==f.geometry.bytes);
        assert(memcmp(replay.geometry.data,f.geometry.data,f.geometry.bytes)==0);
        assert(replay.masked_pixels==f.masked_pixels);
        assert(replay.wall_coverage_pixels==f.wall_coverage_pixels);
        assert(replay.geometry.source_pixels==f.geometry.source_pixels);
        assert(memcmp(replay.visible_bounds,f.visible_bounds,
                      sizeof(f.visible_bounds))==0);
        avp_vk_layered_frame_dispose(&replay);
    }
    /* R376: actual 1080p source-coordinate tape: four workers MUST produce
     * identical bytes, texture bounds and coverage as the serial oracle. */
    {
        AvpWorldScenePacket wide={};
        assert(avp_world_scene_init(&wide,1920u,1080u));
        wide.map_width=wide.map_height=8u;
        wide.maze=(uint8_t*)calloc(64u,8u);
        assert(wide.maze);wide.maze_capacity=512u;
        memcpy(wide.maze,scene.maze,512u);
        memcpy(wide.flat_image,scene.flat_image,sizeof(wide.flat_image));
        memcpy(wide.wall_image,scene.wall_image,sizeof(wide.wall_image));
        memcpy(wide.wall_window_pair,scene.wall_window_pair,sizeof(wide.wall_window_pair));
        memcpy(wide.window_pairs,scene.window_pairs,sizeof(wide.window_pairs));
        memcpy(wide.window_pair_loaded,scene.window_pair_loaded,
               sizeof(wide.window_pair_loaded));
        memcpy(wide.image,scene.image,sizeof(wide.image));
        wide.camera_x=scene.camera_x;wide.camera_y=scene.camera_y;
        wide.forward_x=scene.forward_x;wide.right_y=scene.right_y;
        wide.focal=960.0;
        for(uint32_t x=0u;x<wide.width;x++){
            if((x%2u)==0u)
                assert(avp_world_scene_hit(&wide,x,1.0,.50,3u<<3u,
                    0u,0u,0u,0,64));
            assert(avp_world_scene_hit(&wide,x,2.0,.25,1u,
                    0u,0u,1u,0,64));
            avp_world_scene_end_column(&wide,x,2.0);
        }
        assert(avp_world_scene_complete(&wide));
        AvpVkLayeredFrame serial={};
        AvpVkLayeredFrame parallel={};
        assert(setenv("AVP_HYBRID_SOURCE_THREADS","1",1)==0);
        assert(avp_vk_layered_frame_build(&wide,&state,&serial));
        assert(setenv("AVP_HYBRID_SOURCE_THREADS","4",1)==0);
        assert(avp_vk_layered_frame_build(&wide,&state,&parallel));
        unsetenv("AVP_HYBRID_SOURCE_THREADS");
        /* The old exhaustive row-by-row replay also agrees at 1080p. */
        AvpVkLayeredFrame legacy={};
        assert(setenv("AVP_HYBRID_SOURCE_REPLAY","1",1)==0);
        assert(avp_vk_layered_frame_build(&wide,&state,&legacy));
        unsetenv("AVP_HYBRID_SOURCE_REPLAY");
        assert(legacy.geometry.bytes==serial.geometry.bytes);
        assert(memcmp(legacy.geometry.data,serial.geometry.data,
                      legacy.geometry.bytes)==0);
        assert(legacy.masked_pixels==serial.masked_pixels);
        assert(legacy.wall_coverage_pixels==serial.wall_coverage_pixels);
        assert(memcmp(legacy.visible_bounds,serial.visible_bounds,
                      sizeof(serial.visible_bounds))==0);
        avp_vk_layered_frame_dispose(&legacy);
        assert(serial.geometry.bytes==parallel.geometry.bytes);
        assert(memcmp(serial.geometry.data,parallel.geometry.data,
                      serial.geometry.bytes)==0);
        assert(serial.masked_pixels==parallel.masked_pixels);
        assert(serial.wall_coverage_pixels==parallel.wall_coverage_pixels);
        assert(serial.geometry.source_pixels==parallel.geometry.source_pixels);
        assert(serial.slots.used_count==parallel.slots.used_count);
        assert(memcmp(serial.visible_bounds,parallel.visible_bounds,
                      sizeof(serial.visible_bounds))==0);
        assert(avp_vk_layered_frame_validate(&serial));
        assert(avp_vk_layered_frame_validate(&parallel));
        /* R379: real 1080p parity workers must sum ALL error counts across
         * row bands, not just compare the first region or ignore failures.
         * Construct the independent legacy oracle, then perturb one pixel
         * in each of four disjoint worker bands. */
        std::vector<uint8_t> parity_rgba((size_t)wide.width*wide.height*4u);
        assert(avp_vk_layered_frame_oracle_make(&serial,
            AVP_VK_WALL_PIXEL_RGBA8,parity_rgba.data(),parity_rgba.size()));
        std::vector<uint32_t> parity_world((size_t)wide.width*wide.height);
        for(size_t i=0;i<parity_world.size();i++)
            parity_world[i]=((uint32_t)parity_rgba[4u*i]<<16u)|
                ((uint32_t)parity_rgba[4u*i+1u]<<8u)|parity_rgba[4u*i+2u];
        uint32_t parity_diff=UINT32_MAX;
        for(const char *workers: {"1","4"}){
            assert(setenv("AVP_HYBRID_SOURCE_THREADS",workers,1)==0);
            assert(avp_vk_layered_frame_validated_tape_matches_world(&parallel,
                parity_world.data(),wide.width,wide.width,wide.height,
                &parity_diff)&&parity_diff==0u);
            for(uint32_t i=0u;i<4u;i++){
                const size_t at=(size_t)(i*wide.height/4u)*wide.width+i;
                parity_world[at]^=1u;
            }
            assert(!avp_vk_layered_frame_validated_tape_matches_world(&parallel,
                parity_world.data(),wide.width,wide.width,wide.height,
                &parity_diff)&&parity_diff==4u);
            for(uint32_t i=0u;i<4u;i++){
                const size_t at=(size_t)(i*wide.height/4u)*wide.width+i;
                parity_world[at]^=1u;
            }
        }
        unsetenv("AVP_HYBRID_SOURCE_THREADS");
        avp_vk_layered_frame_dispose(&serial);
        avp_vk_layered_frame_dispose(&parallel);
        avp_world_scene_destroy(&wide);
    }
    /* Partially transparent original PNG source is still rejected. */
    avp_vk_layered_frame_dispose(&f);
    memset(window_alpha.data(),128,window_alpha.size());
    assert(!avp_vk_layered_frame_build(&scene,&state,&f));
    assert(!f.geometry.data&&!f.layers.column_start);
    avp_vk_layered_frame_dispose(&f);
    avp_world_scene_destroy(&scene);
    std::puts("R358 source-layer visibility (doors/windows/portals), binary alpha, CRY/RGB and pixel oracle PASS");
    return 0;
}
