/* RE47 R355: exact source coordinate lookup for Jaguar floors/ceilings.
 * The original host Software row raster remains authoritative; this module
 * never uploads/reads a CPU-rendered color or changes production rendering. */
#include "pc/pc_vulkan_flat_projection.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/floor_cell_sampling.h"
#include "jaguar/avp_gpu/texture_filter.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void flat_write(uint8_t *p,uint32_t v)
{
    for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(v>>(i*8u));
}
static uint32_t flat_read(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
void avp_vk_flat_projection_dispose(AvpVkFlatProjection *flat)
{
    if(flat){free(flat->data);memset(flat,0,sizeof(*flat));}
}
static int flat_header(const uint8_t *wire,size_t bytes,
                       uint32_t *w,uint32_t *h)
{
    uint32_t width,height;
    if(!wire||bytes<AVP_VK_FLAT_HEADER_BYTES ||
       flat_read(wire)!=AVP_VK_FLAT_MAGIC ||
       flat_read(wire+4u)!=AVP_VK_FLAT_VERSION ||
       flat_read(wire+16u)!=AVP_VK_FLAT_PIXEL_BYTES ||
       flat_read(wire+20u)!=AVP_VK_FLAT_HEADER_BYTES ||
       flat_read(wire+28u)!=0u)return 0;
    width=flat_read(wire+8u);height=flat_read(wire+12u);
    if(!width||!height||width>AVP_WORLD_SCENE_MAX_WIDTH||
       height>AVP_WORLD_SCENE_MAX_HEIGHT ||
       (uint64_t)width*height*AVP_VK_FLAT_PIXEL_BYTES+
         AVP_VK_FLAT_HEADER_BYTES!=(uint64_t)bytes ||
       bytes>AVP_VK_FLAT_MAX_BYTES)return 0;
    if(w)*w=width;
    if(h)*h=height;
    return 1;
}
int avp_vk_flat_projection_get(const uint8_t *wire,size_t bytes,
                                uint32_t x,uint32_t y,AvpVkFlatTexel *out)
{
    uint32_t w,h;const uint8_t *p;
    if(!out||!flat_header(wire,bytes,&w,&h)||x>=w||y>=h)return 0;
    p=wire+AVP_VK_FLAT_HEADER_BYTES+((size_t)y*w+x)*16u;
    out->slot=flat_read(p);out->source_x=flat_read(p+4u);
    out->source_y=flat_read(p+8u);out->cry_shade=(int32_t)flat_read(p+12u);
    return 1;
}
int avp_vk_flat_projection_validate(const uint8_t *wire,size_t bytes,
                                     const AvpWorldScenePacket *scene)
{
    uint32_t w,h,active=0u;
    if(!scene||!flat_header(wire,bytes,&w,&h)||
       w!=scene->width||h!=scene->height ||
       !avp_world_scene_complete(scene))return 0;
    for(size_t i=0u,count=(size_t)w*h;i<count;i++){
        const uint8_t *p=wire+AVP_VK_FLAT_HEADER_BYTES+i*16u;
        uint32_t slot=flat_read(p);
        if(slot==AVP_VK_FLAT_NO_IMAGE){
            if(flat_read(p+4u)||flat_read(p+8u)||flat_read(p+12u))return 0;
            continue;
        }
        if(slot>=AVP_WORLD_SCENE_PANELS ||
           scene->image[slot].encoding==AVP_WORLD_IMAGE_ABSENT ||
           (scene->image[slot].encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
            scene->image[slot].encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8) ||
           flat_read(p+4u)>=scene->image[slot].width ||
           flat_read(p+8u)>=scene->image[slot].height ||
           (int32_t)flat_read(p+12u)<-255 ||
           (int32_t)flat_read(p+12u)>255)return 0;
        active++;
    }
    return active==flat_read(wire+24u);
}
int avp_vk_flat_projection_build(const AvpWorldScenePacket *s,
                                  AvpVkFlatProjection *out)
{
    size_t bytes;uint8_t *data;uint8_t seen[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint32_t active=0u,slots=0u;
    if(!out||out->data||out->bytes||out->pixels||out->source_pixels||
       out->source_slots || !s || !avp_world_scene_complete(s) ||
       !s->width||!s->height||!s->maze||
       s->width>AVP_WORLD_SCENE_MAX_WIDTH||
       s->height>AVP_WORLD_SCENE_MAX_HEIGHT ||
       s->map_width>AVP_WORLD_SCENE_MAX_MAP_DIM||
       s->map_height>AVP_WORLD_SCENE_MAX_MAP_DIM ||
       s->maze_capacity<(size_t)s->map_width*s->map_height*8u ||
       !isfinite(s->camera_x)||!isfinite(s->camera_y)||
       !isfinite(s->forward_x)||!isfinite(s->forward_y)||
       !isfinite(s->right_x)||!isfinite(s->right_y)||
       !isfinite(s->focal)||s->focal<=0.0||
       !isfinite(s->stereo_shift)||!isfinite(s->editor_pitch) ||
       ((s->filter_mask&(1u<<AVP_FILTER_GEOMETRY)) &&
        (s->filter_mode||s->anisotropic_taps>1u)))return 0;
    if((uint64_t)s->width*s->height*AVP_VK_FLAT_PIXEL_BYTES+
       AVP_VK_FLAT_HEADER_BYTES>AVP_VK_FLAT_MAX_BYTES)return 0;
    bytes=AVP_VK_FLAT_HEADER_BYTES+(size_t)s->width*s->height*16u;
    data=(uint8_t*)calloc(1u,bytes);
    if(!data)return 0;
    flat_write(data,AVP_VK_FLAT_MAGIC);
    flat_write(data+4u,AVP_VK_FLAT_VERSION);
    flat_write(data+8u,s->width);flat_write(data+12u,s->height);
    flat_write(data+16u,AVP_VK_FLAT_PIXEL_BYTES);
    flat_write(data+20u,AVP_VK_FLAT_HEADER_BYTES);
    /* The Jaguar leaves a deliberate black horizon band. Unvisited rows
     * MUST be encoded as absent, not as source-image slot zero. */
    for(size_t i=0u,n=(size_t)s->width*s->height;i<n;i++)
        flat_write(data+AVP_VK_FLAT_HEADER_BYTES+i*16u,
                   AVP_VK_FLAT_NO_IMAGE);
    for(uint32_t y=0u;y<s->height;y++){
        double row=((double)y+0.5)*228.0/(double)s->height-
            (s->editor_world_only?s->editor_pitch*228.0*0.52:0.0);
        int ceiling=row<114.0;
        double perp;
        if(!avp_host_flat_row_depth(row,(int)s->draw_distance_mode,&perp))continue;
        int shade=avp_host_fog_shade_delta(
            avp_gpu_retail_flat_intensity_delta(
                avp_host_fog_distance(perp,(int)s->fog_distance_mode)),
            (int)s->disable_fog);
        if(shade<-255||shade>255)goto fail;
        /* Mirrors avp_floor_rows' repeated double additions, not an x*step
         * shortcut that can differ at exact cell/texel boundaries. */
        double lefttan=(0.5-(double)s->width*0.5)/s->focal+s->stereo_shift;
        double step=1.0/s->focal;
        double wx=s->camera_x+(s->forward_x+s->right_x*lefttan)*perp;
        double wy=s->camera_y+(s->forward_y+s->right_y*lefttan)*perp;
        double wxstep=s->right_x*step*perp;
        double wystep=s->right_y*step*perp;
        for(uint32_t x=0u;x<s->width;x++,wx+=wxstep,wy+=wystep){
            int cx,cy;
            uint8_t *p=data+AVP_VK_FLAT_HEADER_BYTES+((size_t)y*s->width+x)*16u;
            uint32_t slot=AVP_VK_FLAT_NO_IMAGE;
            flat_write(p,AVP_VK_FLAT_NO_IMAGE);
            if(!avp_gpu_floor_valid_cell(wx,wy,s->map_width,s->map_height,
                                         &cx,&cy))continue;
            uint8_t flat=s->maze[((size_t)cy*s->map_width+(size_t)cx)*8u+
                                  (ceiling?4u:5u)];
            slot=s->flat_image[flat];
            if(slot>=AVP_WORLD_SCENE_PANELS ||
               s->image[slot].encoding==AVP_WORLD_IMAGE_ABSENT)continue;
            uint32_t tw=s->image[slot].width,th=s->image[slot].height;
            if(!tw||!th||tw>8192u||th>8192u ||
               (s->image[slot].encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
                s->image[slot].encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8))goto fail;
            double ux=wx-(double)cx,uy=wy-(double)cy;
            int tx=(int)(ux*tw),ty=(int)(uy*th);
            if(s->flat_rotation[flat] && tw==th){
                int ox=tx,oy=ty;
                switch(s->flat_rotation[flat]){
                case 1:tx=oy;ty=(int)th-1-ox;break;
                case 2:tx=(int)tw-1-ox;ty=(int)th-1-oy;break;
                case 3:tx=(int)tw-1-oy;ty=ox;break;
                default:break;
                }
            }
            /* Never upload an out-of-range texel address to the shader. */
            if(tx<0||ty<0||(uint32_t)tx>=tw||(uint32_t)ty>=th)goto fail;
            flat_write(p,slot);flat_write(p+4u,(uint32_t)tx);
            flat_write(p+8u,(uint32_t)ty);flat_write(p+12u,(uint32_t)shade);
            if(!seen[slot]){seen[slot]=1u;slots++;}
            active++;
        }
    }
    flat_write(data+24u,active);
    out->data=data;out->bytes=bytes;
    out->pixels=s->width*s->height;
    out->source_pixels=active;out->source_slots=slots;
    return 1;
fail:
    free(data);return 0;
}
