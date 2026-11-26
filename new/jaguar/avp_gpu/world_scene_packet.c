#include "jaguar/avp_gpu/world_scene_packet.h"
#include "jaguar/avp_gpu/state.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int avp_world_image_valid(const AvpGpuRgbImage *image)
{
    return image && image->rgb && image->width>0 && image->height>0 &&
           image->width<=8192 && image->height<=8192 &&
           (size_t)image->width <= SIZE_MAX/(size_t)image->height/4u;
}

const AvpGpuRgbImage *avp_world_scene_source(const AvpAvpGpuState *state,
                                             uint32_t slot)
{
    const AvpGpuRgbImage *src;
    const int hires=state && state->host_world_tex_enabled &&
                    *state->host_world_tex_enabled;
    if (!state || slot>=AVP_WORLD_SCENE_IMAGE_SLOTS) return NULL;
    if (slot<AVP_WORLD_SCENE_PANELS) {
        /* Match the current software renderer: panel 16 gets the original
         * sign overlay unless the wall is an unnumbered/non-sign panel. */
        if (!avp_world_image_valid(&state->panel_tex[slot])) return NULL;
        src=&state->panel_tex[slot];
        if (hires && state->host_world_tex &&
            avp_world_image_valid(&state->host_world_tex[slot]))
            src=&state->host_world_tex[slot];
    } else {
        uint32_t index=slot-AVP_WORLD_SCENE_PANELS;
        src=&state->notice_tex[index];
        if (hires && state->host_notice_tex &&
            avp_world_image_valid(&state->host_notice_tex[index]))
            src=&state->host_notice_tex[index];
        if (!avp_world_image_valid(src)) return NULL;
    }
    return src;
}

int avp_world_scene_init(AvpWorldScenePacket *p,uint32_t width,uint32_t height)
{
    size_t hit_count;
    if (!p || !width || !height || width>AVP_WORLD_SCENE_MAX_WIDTH ||
        height>AVP_WORLD_SCENE_MAX_HEIGHT) return 0;
    memset(p,0,sizeof(*p));
    hit_count=(size_t)width*AVP_WORLD_SCENE_MAX_HITS;
    if (hit_count>SIZE_MAX/sizeof(AvpWorldRayHit))return 0;
    p->columns=calloc(width,sizeof(*p->columns));
    p->hits=calloc(hit_count,sizeof(*p->hits));
    if (!p->columns || !p->hits) {
        avp_world_scene_destroy(p);return 0;
    }
    p->width=width;p->height=height;
    p->hit_capacity_per_column=AVP_WORLD_SCENE_MAX_HITS;
    p->image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    return 1;
}

void avp_world_scene_destroy(AvpWorldScenePacket *p)
{
    if (!p)return;
    free(p->columns);free(p->hits);free(p->maze);
    memset(p,0,sizeof(*p));
}

void avp_world_scene_invalidate(AvpWorldScenePacket *p)
{
    if (!p)return;
    /* An old complete packet must never be accidentally consumed for a
     * computer-screen frame, failed allocation, or invalid viewpoint. */
    p->map_width=p->map_height=0u;
    if (p->columns)
        memset(p->columns,0,(size_t)p->width*sizeof(*p->columns));
}

int avp_world_scene_begin(AvpWorldScenePacket *p,const AvpAvpGpuState *state,
                          const uint8_t *maze,uint32_t mw,uint32_t mh,
                          uint32_t level,double px,double py,double fx,double fy,
                          double rx,double ry,double focal)
{
    size_t bytes;
    uint16_t wall[256],flat[256],window[256];
    uint8_t rotation[256];
    AvpWorldImageMeta images[AVP_WORLD_SCENE_IMAGE_SLOTS];
    const int hires=state && state->host_world_tex_enabled &&
                    *state->host_world_tex_enabled;
    /* R331: once a new frame begins, never expose a completed prior frame
     * if validation or maze reallocation fails. Vulkan must not replay stale
     * ray hits after a rejected frame. */
    if (!p) return 0;
    avp_world_scene_invalidate(p);
    if (!p->columns || !p->hits || !p->width || !p->height || !state || !maze ||
        !mw || !mh || mw>AVP_WORLD_SCENE_MAX_MAP_DIM ||
        mh>AVP_WORLD_SCENE_MAX_MAP_DIM || level<1u || level>15u ||
        !isfinite(px) || !isfinite(py) || !isfinite(fx) || !isfinite(fy) ||
        !isfinite(rx) || !isfinite(ry) || !isfinite(focal) || focal<=0.0)
        return 0;
    bytes=(size_t)mw*mh*8u;
    /* Validate and reserve BEFORE changing the current packet. */
    if (p->maze_capacity<bytes){
        uint8_t *new_map=realloc(p->maze,bytes);
        if (!new_map)return 0;
        p->maze=new_map;p->maze_capacity=bytes;
    }
    for (uint32_t slot=0;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++) {
        const AvpGpuRgbImage *src=avp_world_scene_source(state,slot);
        images[slot].width=src?(uint32_t)src->width:0u;
        images[slot].height=src?(uint32_t)src->height:0u;
        images[slot].encoding=src ? (src->cry16 ?
            AVP_WORLD_IMAGE_CRY16_ALPHA8:AVP_WORLD_IMAGE_RGB8_ALPHA8):
            AVP_WORLD_IMAGE_ABSENT;
    }
    for (uint32_t i=0;i<256u;i++) {
        unsigned file=0,index=0;
        wall[i]=flat[i]=window[i]=AVP_WORLD_SCENE_INVALID_SLOT;
        rotation[i]=(uint8_t)avp_gpu_flat_texture_rotation(level,(uint8_t)i);
        if (avp_gpu_wall_texture_file(level,(uint8_t)i,&file) &&
            file<AVP_WORLD_SCENE_PANELS &&
            avp_world_image_valid(&state->panel_tex[file])) {
            uint32_t slot=file;
            if (file==16u && avp_gpu_wall_notice_index(level,(uint8_t)i,&index) &&
                index<AVP_WORLD_SCENE_SIGNS &&
                (avp_world_image_valid(&state->notice_tex[index]) ||
                 (hires && state->host_notice_tex &&
                  avp_world_image_valid(&state->host_notice_tex[index]))))
                slot=AVP_WORLD_SCENE_PANELS+index;
            wall[i]=(uint16_t)slot;
        }
        if (avp_gpu_flat_texture_file(level,(uint8_t)i,&file) &&
            file<AVP_WORLD_SCENE_PANELS &&
            avp_world_image_valid(&state->panel_tex[file]))
            flat[i]=(uint16_t)file;
        if (avp_gpu_wall_window_pair_file(level,(uint8_t)i,&file) &&
            file>=74u && file<=86u && state->window_pair_loaded[file-74u])
            window[i]=(uint16_t)(file-74u); /* index into window_pairs */
    }
    memcpy(p->maze,maze,bytes);
    p->map_width=mw;p->map_height=mh;p->logical_level=level;
    p->camera_x=px;p->camera_y=py;p->forward_x=fx;p->forward_y=fy;
    p->right_x=rx;p->right_y=ry;p->focal=focal;
    p->stereo_shift=state->host_stereo_projection_shift;
    p->filter_mode=state->host_filter_mode;
    p->anisotropic_taps=state->host_aniso;
    p->filter_mask=state->host_filter_mask;
    p->occlusion_enabled=0u; /* filled once the wall occlusion buffer exists */
    p->disable_fog=(uint32_t)!!state->host_disable_fog;
    p->draw_distance_mode=(uint32_t)state->host_extend_draw_distance;
    p->fog_distance_mode=(uint32_t)state->host_extend_fog_distance;
    p->editor_world_only=(uint32_t)!!state->map_editor_world_only;
    p->editor_pitch=state->map_editor_pitch;
    memcpy(p->image,images,sizeof(images));
    memcpy(p->wall_image,wall,sizeof(wall));
    memcpy(p->flat_image,flat,sizeof(flat));
    memcpy(p->wall_window_pair,window,sizeof(window));
    memcpy(p->flat_rotation,rotation,sizeof(rotation));
    memcpy(p->window_pairs,state->window_pairs,sizeof(p->window_pairs));
    memcpy(p->window_pair_loaded,state->window_pair_loaded,sizeof(p->window_pair_loaded));
    memset(p->columns,0,(size_t)p->width*sizeof(*p->columns));
    return 1;
}

int avp_world_scene_hit(AvpWorldScenePacket *p,uint32_t x,
                        double distance,double tc,uint32_t wall,uint32_t open,
                        uint32_t vertical,uint32_t is_final,int32_t lo,int32_t hi)
{
    AvpWorldRayColumn *column;
    AvpWorldRayHit *hit;
    if (!p || !p->columns || !p->hits || x>=p->width ||
        !isfinite(distance) || distance<0.0 || !isfinite(tc) ||
        wall>255u || open>255u || vertical>1u || is_final>1u) {
        if (p && p->columns && x<p->width)
            p->columns[x].overflow=1u; /* never silently omit a ray */
        return 0;
    }
    column=&p->columns[x];
    if (column->ready || column->overflow ||
        column->count>=p->hit_capacity_per_column) {
        column->overflow=1u;return 0;
    }
    hit=&p->hits[(size_t)x*p->hit_capacity_per_column+column->count++];
    hit->distance=distance;hit->texture_fraction=tc;
    hit->wall=wall;hit->open=open;hit->vertical=vertical;
    hit->is_final=is_final;hit->clip_lo=lo;hit->clip_hi=hi;
    return 1;
}

void avp_world_scene_end_column(AvpWorldScenePacket *p,uint32_t x,double depth)
{
    if (!p || !p->columns || x>=p->width)return;
    if (!isfinite(depth) || depth<0.0) {p->columns[x].overflow=1u;return;}
    p->columns[x].wall_depth=depth;
    p->columns[x].ready=1u;
}

int avp_world_scene_complete(const AvpWorldScenePacket *p)
{
    if (!p || !p->maze || !p->columns || !p->hits ||
        !p->map_width || !p->map_height)return 0;
    for (uint32_t x=0;x<p->width;x++) {
        const AvpWorldRayColumn *c=&p->columns[x];
        if (!c->ready || c->overflow ||
            c->count>p->hit_capacity_per_column) return 0;
    }
    return 1;
}

int avp_world_scene_pack_image(const AvpGpuRgbImage *src,
                               uint32_t *dst,size_t capacity,
                               AvpWorldImageEncoding *encoding)
{
    size_t pixels;
    if (!avp_world_image_valid(src) || !dst || !encoding)return 0;
    pixels=(size_t)src->width*(size_t)src->height;
    if (capacity<pixels)return 0;
    if (src->cry16) {
        *encoding=AVP_WORLD_IMAGE_CRY16_ALPHA8;
        for (size_t i=0;i<pixels;i++)
            dst[i]=(uint32_t)src->cry16[i] |
                ((uint32_t)(src->alpha?src->alpha[i]:255u)<<16u);
    } else {
        *encoding=AVP_WORLD_IMAGE_RGB8_ALPHA8;
        for (size_t i=0;i<pixels;i++) {
            const uint8_t *c=src->rgb+i*3u;
            dst[i]=((uint32_t)(src->alpha?src->alpha[i]:255u)<<24u)|
                   ((uint32_t)c[0]<<16u)|((uint32_t)c[1]<<8u)|c[2];
        }
    }
    return 1;
}
