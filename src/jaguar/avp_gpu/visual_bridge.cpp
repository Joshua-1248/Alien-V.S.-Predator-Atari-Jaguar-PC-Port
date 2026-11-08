#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/jagpeg.hpp"
#include "jaguar/avp_gpu/panels.hpp"
#include "jaguar/avp_gpu/doors.hpp"
#include "jaguar/avp_gpu/cry.hpp"
#include "jaguar/avp_gpu/projection.hpp"
#include "jaguar/cry.h"
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" int avp_gpu_decode_jagpeg(const u8 *data,size_t size,int quality,int corrected_chroma,AvpGpuRgbImage *out)
{
    if(!data||!size||!out)return 0;
    try{
        std::vector<std::uint8_t> src(data,data+size);
        auto im=avp::decode_jagpeg(src,quality,corrected_chroma!=0);
        const size_t n=(size_t)im.width*(size_t)im.height*3u;
        auto *p=(u8*)std::malloc(n?n:1u);
        if(!p)return 0;
        std::memcpy(p,im.rgb.data(),n);
        out->width=im.width;out->height=im.height;out->rgb=p;out->cry16=nullptr;return 1;
    }catch(...){return 0;}
}
extern "C" void avp_gpu_free_rgb_image(AvpGpuRgbImage *img){if(img){std::free(img->rgb);std::free(img->cry16);img->rgb=nullptr;img->cry16=nullptr;img->width=img->height=0;}}
extern "C" int avp_gpu_build_cry_image(AvpGpuRgbImage *img,const u8 *qconv,size_t qconv_size)
{
    size_t count,i;u16 *cry;
    if(!img||!img->rgb||img->width<=0||img->height<=0||!qconv||qconv_size<131072u)return 0;
    count=(size_t)img->width*(size_t)img->height;
    cry=(u16*)std::malloc(count*sizeof(*cry));
    if(!cry)return 0;
    for(i=0;i<count;i++){const u8 *p=img->rgb+i*3u;cry[i]=avp_jaguar_rgb_to_cry_qconv(p[0],p[1],p[2],qconv,qconv_size);}
    std::free(img->cry16);img->cry16=cry;return 1;
}
extern "C" int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file_id){auto v=avp::wall_texture_file(level,wall);if(!v)return 0;if(file_id)*file_id=(unsigned)*v;return 1;}
extern "C" int avp_gpu_wall_notice_index(unsigned level,u8 wall,unsigned *notice_index){
    const auto info=avp::wall_notice_overlay(level,wall);
    if(!info)return 0;
    /* Semantic notice index is stable even if its artwork FILES ID differs. */
    static constexpr unsigned overlay_files[13]={62,57,61,59,58,60,56,64,63,67,66,68,65};
    for(unsigned i=0;i<13u;i++)if(overlay_files[i]==info->overlay_file){
        unsigned ni=i;
        if(i==0u&&info->number_file&&level>=1u&&level<=5u)ni=13u+level-1u;
        if(notice_index)*notice_index=ni;
        return 1;
    }
    return 0;
}
extern "C" int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file_id){auto v=avp::flat_texture_file(level,flat);if(!v)return 0;if(file_id)*file_id=(unsigned)*v;return 1;}
extern "C" unsigned avp_gpu_flat_texture_rotation(unsigned level,u8 flat){return avp::flat_texture_rotation(level,flat);}

extern "C" int avp_gpu_wall_window_pair_file(unsigned level,u8 wall,unsigned *file_id){auto v=avp::wall_window_pair_file(level,wall);if(!v)return 0;if(file_id)*file_id=(unsigned)*v;return 1;}
extern "C" int avp_gpu_door_visual_column_open(u8 wall,u8 open,int panel_pos){return avp::door_visual_column_open(wall,open,panel_pos)?1:0;}
extern "C" unsigned avp_gpu_door_opening_limit(u8 wall){return (unsigned)avp::DoorSystem::opening_limit(wall);}
extern "C" int avp_gpu_retail_wall_intensity_delta(double d){return avp::retail_wall_intensity_delta(d);}
extern "C" int avp_gpu_retail_flat_intensity_delta(double d){return avp::retail_flat_intensity_delta(d);}
