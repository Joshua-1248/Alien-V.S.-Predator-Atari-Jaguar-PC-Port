/* R355: compare exact source-coordinate AVFL records against the Jaguar
 * Software floor-row algorithm. Does not require Vulkan, ROM, or game art. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "pc/pc_vulkan_flat_projection.h"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/floor_cell_sampling.h"
}
#include "jaguar/avp_gpu/projection.hpp"
extern "C" int avp_gpu_retail_flat_intensity_delta(double distance)
{
    return avp::retail_flat_intensity_delta(distance);
}
static uint32_t rd(const uint8_t *p)
{
    return uint32_t(p[0])|(uint32_t(p[1])<<8u)|
           (uint32_t(p[2])<<16u)|(uint32_t(p[3])<<24u);
}
static bool reference(const AvpWorldScenePacket &s,uint32_t x,uint32_t y,
                      AvpVkFlatTexel &out)
{
    out={AVP_VK_FLAT_NO_IMAGE,0u,0u,0};
    double row=(double(y)+0.5)*228.0/double(s.height)-
        (s.editor_world_only?s.editor_pitch*228.0*0.52:0.0);
    double depth=0.0;
    if(!avp_host_flat_row_depth(row,int(s.draw_distance_mode),&depth))return false;
    int shade=avp_gpu_retail_flat_intensity_delta(
        avp_host_fog_distance(depth,int(s.fog_distance_mode)));
    if(s.disable_fog)shade=0;
    double tan=(0.5-double(s.width)*0.5)/s.focal+s.stereo_shift;
    double wx=s.camera_x+(s.forward_x+s.right_x*tan)*depth;
    double wy=s.camera_y+(s.forward_y+s.right_y*tan)*depth;
    double dx=s.right_x*(1.0/s.focal)*depth;
    double dy=s.right_y*(1.0/s.focal)*depth;
    for(uint32_t i=0;i<x;i++){wx+=dx;wy+=dy;}
    int cx=0,cy=0;
    if(!avp_gpu_floor_valid_cell(wx,wy,s.map_width,s.map_height,&cx,&cy))return false;
    uint8_t flat=s.maze[((size_t)cy*s.map_width+(size_t)cx)*8u+
                        (row<114.0?4u:5u)];
    uint32_t slot=s.flat_image[flat];
    if(slot>=AVP_WORLD_SCENE_PANELS ||
       s.image[slot].encoding==AVP_WORLD_IMAGE_ABSENT)return false;
    const uint32_t w=s.image[slot].width,h=s.image[slot].height;
    int u=int((wx-double(cx))*w),v=int((wy-double(cy))*h);
    if(s.flat_rotation[flat]&&w==h){
        int a=u,b=v;
        switch(s.flat_rotation[flat]){
        case 1:u=b;v=int(h)-1-a;break;
        case 2:u=int(w)-1-a;v=int(h)-1-b;break;
        case 3:u=int(w)-1-b;v=a;break;
        }
    }
    assert(u>=0&&v>=0&&uint32_t(u)<w&&uint32_t(v)<h);
    out={slot,uint32_t(u),uint32_t(v),shade};
    return true;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    AvpWorldScenePacket s={};
    assert(avp_world_scene_init(&s,64u,48u));
    s.map_width=s.map_height=8u;
    s.maze=(uint8_t*)calloc(8u*8u,8u);
    assert(s.maze);
    s.maze_capacity=8u*8u*8u;
    for(unsigned j=0;j<64u;j++){
        s.maze[j*8u+4u]=(uint8_t)((j&1u)?3u:1u);
        s.maze[j*8u+5u]=(uint8_t)((j&1u)?4u:2u);
    }
    for(auto &image:s.flat_image)image=AVP_WORLD_SCENE_INVALID_SLOT;
    s.flat_image[1]=10u;s.flat_image[2]=11u;
    s.flat_image[3]=12u;s.flat_image[4]=13u;
    s.image[10]={16u,16u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    s.image[11]={32u,8u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    s.image[12]={17u,17u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    s.image[13]={16u,16u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    s.camera_x=s.camera_y=3.5;
    s.forward_x=1.0;s.forward_y=0.0;
    s.right_x=0.0;s.right_y=1.0;
    s.focal=double(s.width)*0.5;
    s.draw_distance_mode=0u;s.fog_distance_mode=0u;
    for(uint32_t x=0;x<s.width;x++)s.columns[x].ready=1u;
    AvpVkFlatProjection f={};
    assert(avp_vk_flat_projection_build(&s,&f));
    assert(!avp_vk_flat_projection_build(&s,&f));
    assert(f.pixels==64u*48u&&f.source_pixels>0&&f.source_slots>=2u);
    assert(rd(f.data)==AVP_VK_FLAT_MAGIC);
    assert(avp_vk_flat_projection_validate(f.data,f.bytes,&s));
    for(uint32_t y=0;y<s.height;y++){
        for(uint32_t x=0;x<s.width;x++){
            AvpVkFlatTexel got={},want={};
            assert(avp_vk_flat_projection_get(f.data,f.bytes,x,y,&got));
            reference(s,x,y,want);
            assert(got.slot==want.slot);
            assert(got.source_x==want.source_x);
            assert(got.source_y==want.source_y);
            assert(got.cry_shade==want.cry_shade);
        }
    }
    assert(!avp_vk_flat_projection_get(f.data,f.bytes,64u,1u,nullptr));
    {std::vector<uint8_t> tampered(f.data,f.data+f.bytes);
     tampered[0]=0u;assert(!avp_vk_flat_projection_validate(tampered.data(),tampered.size(),&s));
     tampered[0]=f.data[0];tampered[24]^=1u;
     assert(!avp_vk_flat_projection_validate(tampered.data(),tampered.size(),&s));
     tampered[24]=f.data[24];
     auto it=tampered.begin()+AVP_VK_FLAT_HEADER_BYTES;
     while(it!=tampered.end() && rd(&*it)==AVP_VK_FLAT_NO_IMAGE)it+=16u;
     assert(it!=tampered.end());
     *it=0xfe;*(it+1)=0x00;
     assert(!avp_vk_flat_projection_validate(tampered.data(),tampered.size(),&s));
    }
    avp_vk_flat_projection_dispose(&f);
    s.filter_mask=(1u<<3u);s.filter_mode=2u;
    assert(!avp_vk_flat_projection_build(&s,&f)&&f.data==nullptr);
    s.filter_mask=0u;s.filter_mode=0u;
    for(unsigned rotation=0u;rotation<4u;rotation++){
        s.flat_rotation[1]=(uint8_t)rotation;s.flat_rotation[3]=(uint8_t)rotation;
        s.editor_world_only=rotation&1u;s.editor_pitch=0.02*rotation;
        s.draw_distance_mode=rotation?2u:0u;
        s.fog_distance_mode=rotation%4u;
        s.disable_fog=rotation==3u;
        assert(avp_vk_flat_projection_build(&s,&f));
        for(uint32_t y=0;y<s.height;y++)for(uint32_t x=0;x<s.width;x++){
            AvpVkFlatTexel got={},want={};
            assert(avp_vk_flat_projection_get(f.data,f.bytes,x,y,&got));
            reference(s,x,y,want);
            assert(got.slot==want.slot&&got.source_x==want.source_x&&
                   got.source_y==want.source_y&&got.cry_shade==want.cry_shade);
        }
        avp_vk_flat_projection_dispose(&f);
    }
    FILE *shader=fopen(argv[1],"rb");assert(shader);
    assert(fseek(shader,0,SEEK_END)==0);
    long len=ftell(shader);assert(len>100);
    assert(fseek(shader,0,SEEK_SET)==0);
    std::vector<char> source(size_t(len)+1u,0);
    assert(fread(source.data(),1,size_t(len),shader)==size_t(len));fclose(shader);
    assert(strstr(source.data(),"0x4c465641u"));
    assert(strstr(source.data(),"avp_cry_rgb"));
    assert(strstr(source.data(),"texelFetch(avp_source"));
    puts("R355 exact Jaguar nearest floor/ceiling source-coordinate stream PASS");
    avp_world_scene_destroy(&s);
    return 0;
}
