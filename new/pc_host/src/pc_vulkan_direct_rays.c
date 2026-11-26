#include "pc/pc_vulkan_direct_rays.h"
#include <string.h>
#include <stdint.h>

int avp_vk_direct_rays_match_capture(const AvpWorldScenePacket *a,
                                     const AvpWorldScenePacket *b)
{
    size_t map_bytes;
    if(!a||!b || !a->width || !a->height ||
       a->width>AVP_WORLD_SCENE_MAX_WIDTH ||
       a->height>AVP_WORLD_SCENE_MAX_HEIGHT ||
       a->image_slot_count!=AVP_WORLD_SCENE_IMAGE_SLOTS ||
       a->hit_capacity_per_column!=AVP_WORLD_SCENE_MAX_HITS ||
       !avp_world_scene_complete(a) ||
       !avp_world_scene_complete(b) || a->width!=b->width ||
       a->height!=b->height || a->map_width!=b->map_width ||
       a->map_height!=b->map_height ||
       a->logical_level!=b->logical_level ||
       a->image_slot_count!=b->image_slot_count ||
       a->hit_capacity_per_column!=b->hit_capacity_per_column ||
       a->filter_mode!=b->filter_mode ||
       a->anisotropic_taps!=b->anisotropic_taps ||
       a->filter_mask!=b->filter_mask ||
       a->disable_fog!=b->disable_fog ||
       a->draw_distance_mode!=b->draw_distance_mode ||
       a->fog_distance_mode!=b->fog_distance_mode ||
       a->editor_world_only!=b->editor_world_only ||
       a->editor_pitch!=b->editor_pitch ||
       a->camera_x!=b->camera_x || a->camera_y!=b->camera_y ||
       a->forward_x!=b->forward_x || a->forward_y!=b->forward_y ||
       a->right_x!=b->right_x || a->right_y!=b->right_y ||
       a->focal!=b->focal || a->stereo_shift!=b->stereo_shift ||
       a->map_width>SIZE_MAX/8u/a->map_height)return 0;
    /* A direct ray trace intentionally does not allocate Software wall
     * occlusion. Ignore occlusion_enabled: it is a raster-side artifact,
     * not a property of source visibility. */
    map_bytes=(size_t)a->map_width*a->map_height*8u;
    if(a->maze_capacity<map_bytes||b->maze_capacity<map_bytes||
       memcmp(a->maze,b->maze,map_bytes) ||
       memcmp(a->wall_image,b->wall_image,sizeof(a->wall_image)) ||
       memcmp(a->flat_image,b->flat_image,sizeof(a->flat_image)) ||
       memcmp(a->wall_window_pair,b->wall_window_pair,sizeof(a->wall_window_pair)) ||
       memcmp(a->flat_rotation,b->flat_rotation,sizeof(a->flat_rotation)) ||
       memcmp(a->window_pairs,b->window_pairs,sizeof(a->window_pairs)) ||
       memcmp(a->window_pair_loaded,b->window_pair_loaded,sizeof(a->window_pair_loaded)))return 0;
    /* Compare image metadata scalars, not padding or texture pointers. */
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if(a->image[i].width!=b->image[i].width ||
           a->image[i].height!=b->image[i].height ||
           a->image[i].encoding!=b->image[i].encoding)return 0;
    for(uint32_t x=0;x<a->width;x++){
        const AvpWorldRayColumn *u=&a->columns[x],*v=&b->columns[x];
        if(u->wall_depth!=v->wall_depth || u->count!=v->count ||
           !u->ready||!v->ready||u->overflow||v->overflow)return 0;
        for(uint32_t j=0u;j<u->count;j++){
            const AvpWorldRayHit *p=&a->hits[(size_t)x*a->hit_capacity_per_column+j];
            const AvpWorldRayHit *q=&b->hits[(size_t)x*b->hit_capacity_per_column+j];
            if(p->distance!=q->distance ||
               p->texture_fraction!=q->texture_fraction ||
               p->wall!=q->wall || p->open!=q->open ||
               p->vertical!=q->vertical || p->is_final!=q->is_final ||
               p->clip_lo!=q->clip_lo || p->clip_hi!=q->clip_hi)return 0;
        }
    }
    return 1;
}
