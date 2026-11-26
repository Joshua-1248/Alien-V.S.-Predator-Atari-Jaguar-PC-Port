/* RE47/R358: binary-alpha source visibility, exact original MAZE2 layer order.
 * This SDK-independent host code NEVER constructs or uploads a Software
 * framebuffer: AVFL records contain only slot/source-x/source-y/CRY shade. */
#include "pc/pc_vulkan_layered_frame.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <pthread.h>
#endif

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static void put32(uint8_t *p,uint32_t v)
{
    for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(v>>(i*8u));
}
static const uint8_t *pixel_at(const AvpVkFlatProjection *p,uint32_t width,
                               uint32_t x,uint32_t y)
{
    return p->data+AVP_VK_FLAT_HEADER_BYTES+((size_t)y*width+x)*16u;
}
static int source_ref(const AvpVkLayeredFrame *f,uint32_t slot,
                      uint32_t sx,uint32_t sy,int32_t shade,
                      uint32_t *rgb,uint8_t *alpha)
{
    const AvpVkWallPixelSource *src;
    uint32_t raw;
    if(slot>=AVP_WORLD_SCENE_IMAGE_SLOTS || !rgb || !alpha ||
       shade< -255 || shade>255)return 0;
    src=&f->pixels[slot];
    if(!src->words||sx>=src->width||sy>=src->height)return 0;
    raw=src->words[(size_t)sy*src->width+sx];
    if(src->encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8){
        int brightness=(int)(raw&255u)+shade;
        if(!f->cry_lut)return 0;
        if(brightness<0)brightness=0;
        if(brightness>255)brightness=255;
        *rgb=f->cry_lut[(raw&0xff00u)|(uint32_t)brightness]&0xffffffu;
        *alpha=(uint8_t)((raw>>16u)&255u);
    }else if(src->encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8){
        /* Matches original host replacement wall+flat path: no CRY shade. */
        *rgb=raw&0xffffffu;
        *alpha=(uint8_t)(raw>>24u);
    }else return 0;
    return 1;
}
static int read_ref(const AvpVkLayeredFrame *f,const uint8_t *p,
                    uint32_t *rgb,uint8_t *alpha)
{
    uint32_t slot=get32(p);
    if(slot==AVP_VK_FLAT_NO_IMAGE){*rgb=0u;*alpha=0u;return 1;}
    return source_ref(f,slot,get32(p+4u),get32(p+8u),
                      (int32_t)get32(p+12u),rgb,alpha);
}
/* R379: The geometry resolver chooses source coordinates and verifies binary
 * coverage, but never consumes RGB. Avoid a CRY lookup and shade arithmetic
 * per candidate texel. The independent Software-world oracle still calls
 * read_ref() and performs the FULL original CRY/RGB color conversion. */
static int source_alpha_only(const AvpVkLayeredFrame *f,uint32_t slot,
                             uint32_t sx,uint32_t sy,int32_t shade,
                             uint8_t *alpha)
{
    const AvpVkWallPixelSource *src;
    uint32_t raw;
    if(!f||!alpha||slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||
       shade< -255||shade>255)return 0;
    src=&f->pixels[slot];
    if(!src->words||sx>=src->width||sy>=src->height)return 0;
    raw=src->words[(size_t)sy*src->width+sx];
    if(src->encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8){
        if(!f->cry_lut)return 0;
        *alpha=(uint8_t)((raw>>16u)&255u);
    }else if(src->encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8){
        *alpha=(uint8_t)(raw>>24u);
    }else return 0;
    return 1;
}
static int read_ref_alpha_only(const AvpVkLayeredFrame *f,const uint8_t *p,
                               uint8_t *alpha)
{
    uint32_t slot=get32(p);
    if(!alpha)return 0;
    if(slot==AVP_VK_FLAT_NO_IMAGE){*alpha=0u;return 1;}
    return source_alpha_only(f,slot,get32(p+4u),get32(p+8u),
                             (int32_t)get32(p+12u),alpha);
}
static int replay_pixel(const AvpVkLayeredFrame *f,uint32_t x,uint32_t y,
                        AvpVkFlatTexel *out,uint32_t *masked,uint32_t *covered)
{
    const uint8_t *base=pixel_at(&f->flats,f->layers.width,x,y);
    AvpVkFlatTexel ref={get32(base),get32(base+4u),get32(base+8u),
                         (int32_t)get32(base+12u)};
    uint32_t rgb=0u;uint8_t alpha=0u;
    if(!read_ref(f,base,&rgb,&alpha))return 0;
    if(alpha!=0u&&alpha!=255u)return 0;
    if(alpha==0u){
        if(ref.slot!=AVP_VK_FLAT_NO_IMAGE && masked)(*masked)++;
        ref=(AvpVkFlatTexel){AVP_VK_FLAT_NO_IMAGE,0u,0u,0};
    }
    /* Source order is FAR -> NEAR, including portals with no final wall. */
    for(uint32_t i=f->layers.column_start[x];
        i<f->layers.column_start[x+1u];i++){
        const AvpVkWallLayerSpan *span=&f->layers.spans[i];
        if(y<span->top||y>=span->bottom)continue;
        uint32_t sy=f->layers.rows[span->row_offset+y-span->top];
        if(!source_ref(f,span->source_slot,span->source_x,sy,
                       span->cry_shade,&rgb,&alpha))return 0;
        if(alpha!=0u && alpha!=255u)return 0;
        if(alpha==0u){if(masked)(*masked)++;continue;}
        ref=(AvpVkFlatTexel){span->source_slot,span->source_x,sy,span->cry_shade};
        if(covered)(*covered)++;
    }
    *out=ref;
    return 1;
}
/* R376: each pixel's original flat + far-to-near wall/portal replay is
 * independent once the scene tapes and packed source texels are complete.
 * Split only the already captured, immutable scene across row bands. Each
 * worker owns disjoint geometry bytes and local counters/bounds: no races,
 * no changes to the Jaguar Software renderer or visibility algorithm. */
typedef struct AvpLayerResolveBand {
    AvpVkLayeredFrame *frame;
    uint32_t y0,y1;
    uint32_t masked,covered,active;
    uint8_t visible[AVP_WORLD_SCENE_IMAGE_SLOTS];
    AvpVkSourceBounds bounds[AVP_WORLD_SCENE_IMAGE_SLOTS];
    int ok;
    int legacy_replay; /* R378 developer differential oracle */
} AvpLayerResolveBand;
static void avp_layer_resolve_rows_legacy(AvpLayerResolveBand *job)
{
    AvpVkLayeredFrame *f=job->frame;
    job->ok=0;
    for(uint32_t y=job->y0;y<job->y1;y++){
        for(uint32_t x=0u;x<f->layers.width;x++){
            AvpVkFlatTexel ref;
            uint8_t *p=f->geometry.data+AVP_VK_FLAT_HEADER_BYTES+
                           ((size_t)y*f->layers.width+x)*16u;
            if(!replay_pixel(f,x,y,&ref,&job->masked,&job->covered))return;
            put32(p,ref.slot);put32(p+4u,ref.source_x);
            put32(p+8u,ref.source_y);put32(p+12u,(uint32_t)ref.cry_shade);
            if(ref.slot!=AVP_VK_FLAT_NO_IMAGE){
                AvpVkSourceBounds *b=&job->bounds[ref.slot];
                if(!job->visible[ref.slot]){
                    b->x0=x;b->y0=y;b->x1=x+1u;b->y1=y+1u;
                }else{
                    if(x<b->x0)b->x0=x;
                    if(y<b->y0)b->y0=y;
                    if(x+1u>b->x1)b->x1=x+1u;
                    if(y+1u>b->y1)b->y1=y+1u;
                }
                job->active++;
                job->visible[ref.slot]=1u;
            }
        }
    }
    job->ok=1;
}
/* R378: identical far->near composition, O(visible span rows) rather than
 * O(pixels * total spans in column). Tiles preserve row-major writes and
 * prevent extra 31.6-MiB allocations. Each worker owns disjoint Y bands. */
static void avp_layer_resolve_rows(AvpLayerResolveBand *job)
{
    enum { TILE_WIDTH=32u };
    AvpVkLayeredFrame *f=job->frame;
    AvpVkFlatTexel *tile=NULL;
    uint32_t band_h=job->y1-job->y0;
    job->ok=0;
    if(job->legacy_replay){
        avp_layer_resolve_rows_legacy(job);
        return;
    }
    if(!band_h||band_h>AVP_VK_FLAT_MAX_BYTES/16u/TILE_WIDTH)return;
    tile=(AvpVkFlatTexel*)malloc((size_t)band_h*TILE_WIDTH*sizeof(*tile));
    if(!tile)return;
    for(uint32_t x0=0u;x0<f->layers.width;x0+=TILE_WIDTH){
        const uint32_t tw=f->layers.width-x0<TILE_WIDTH?
                          f->layers.width-x0:TILE_WIDTH;
        /* First resolve the native flat for each destination pixel. */
        for(uint32_t y=job->y0;y<job->y1;y++){
            const uint32_t off=y-job->y0;
            for(uint32_t dx=0u;dx<tw;dx++){
                const uint8_t *base=pixel_at(&f->flats,f->layers.width,x0+dx,y);
                AvpVkFlatTexel ref={get32(base),get32(base+4u),
                                   get32(base+8u),(int32_t)get32(base+12u)};
                uint8_t alpha=0u;
                if(!read_ref_alpha_only(f,base,&alpha)||
                   (alpha!=0u&&alpha!=255u))goto done;
                if(alpha==0u){
                    if(ref.slot!=AVP_VK_FLAT_NO_IMAGE)job->masked++;
                    ref=(AvpVkFlatTexel){AVP_VK_FLAT_NO_IMAGE,0u,0u,0};
                }
                tile[(size_t)off*tw+dx]=ref;
            }
        }
        /* Replay only source rows that a wall/window/door actually touches,
         * in the original source's far-to-near order. Transparent texels
         * count as masked but never overwrite a previously resolved texel. */
        for(uint32_t dx=0u;dx<tw;dx++){
            const uint32_t x=x0+dx;
            for(uint32_t si=f->layers.column_start[x];
                si<f->layers.column_start[x+1u];si++){
                const AvpVkWallLayerSpan *span=&f->layers.spans[si];
                const uint32_t lo=span->top>job->y0?span->top:job->y0;
                const uint32_t hi=span->bottom<job->y1?span->bottom:job->y1;
                for(uint32_t y=lo;y<hi;y++){
                    const uint32_t sy=f->layers.rows[span->row_offset+y-span->top];
                    uint8_t alpha=0u;
                    if(!source_alpha_only(f,span->source_slot,span->source_x,sy,
                               span->cry_shade,&alpha)||
                       (alpha!=0u&&alpha!=255u))goto done;
                    if(!alpha){job->masked++;continue;}
                    tile[(size_t)(y-job->y0)*tw+dx]=(AvpVkFlatTexel){
                        span->source_slot,span->source_x,sy,span->cry_shade};
                    job->covered++;
                }
            }
        }
        /* Write the authoritative final AVFL reference exactly once per
         * destination pixel, in row-major tile order for good locality. */
        for(uint32_t y=job->y0;y<job->y1;y++){
            uint8_t *p=f->geometry.data+AVP_VK_FLAT_HEADER_BYTES+
                       ((size_t)y*f->layers.width+x0)*16u;
            const AvpVkFlatTexel *src=&tile[(size_t)(y-job->y0)*tw];
            for(uint32_t dx=0u;dx<tw;dx++,p+=16u){
                const AvpVkFlatTexel *ref=&src[dx];
                const uint32_t x=x0+dx;
                put32(p,ref->slot);put32(p+4u,ref->source_x);
                put32(p+8u,ref->source_y);
                put32(p+12u,(uint32_t)ref->cry_shade);
                if(ref->slot!=AVP_VK_FLAT_NO_IMAGE){
                    AvpVkSourceBounds *b=&job->bounds[ref->slot];
                    if(!job->visible[ref->slot]){
                        b->x0=x;b->y0=y;b->x1=x+1u;b->y1=y+1u;
                    }else{
                        if(x<b->x0)b->x0=x;
                        if(y<b->y0)b->y0=y;
                        if(x+1u>b->x1)b->x1=x+1u;
                        if(y+1u>b->y1)b->y1=y+1u;
                    }
                    job->visible[ref->slot]=1u;
                    job->active++;
                }
            }
        }
    }
    job->ok=1;
done:
    free(tile);
}

#if !defined(_WIN32)
static void *avp_layer_resolve_thread(void *opaque)
{
    avp_layer_resolve_rows((AvpLayerResolveBand*)opaque);
    return NULL;
}
#endif
static int avp_layer_resolve_all(AvpVkLayeredFrame *f,
                                 uint8_t visible[AVP_WORLD_SCENE_IMAGE_SLOTS],
                                 uint32_t *active)
{
    AvpLayerResolveBand jobs[4]={{0}};
    unsigned n=1u;
#if !defined(_WIN32)
    pthread_t threads[4];
    int started[4]={0};
    /* Small native frames have less work than thread creation overhead. */
    if((uint64_t)f->layers.width*f->layers.height>=262144u)n=4u;
    /* Developer-only deterministic A/B; the ordinary game auto-selects
     * four workers for high-resolution source-layer frames. */
    {const char *override=getenv("AVP_HYBRID_SOURCE_THREADS");
     if(override&&strcmp(override,"1")==0)n=1u;
     else if(override&&strcmp(override,"4")==0)n=4u;}
#endif
    {const char *legacy=getenv("AVP_HYBRID_SOURCE_REPLAY");
     if(legacy&&strcmp(legacy,"1")==0)
         for(unsigned i=0u;i<n;i++)jobs[i].legacy_replay=1;}
    for(unsigned i=0u;i<n;i++){
        jobs[i].frame=f;
        jobs[i].y0=(uint32_t)((uint64_t)i*f->layers.height/n);
        jobs[i].y1=(uint32_t)((uint64_t)(i+1u)*f->layers.height/n);
    }
#if !defined(_WIN32)
    for(unsigned i=1u;i<n;i++){
        if(pthread_create(&threads[i],NULL,avp_layer_resolve_thread,&jobs[i])==0)
            started[i]=1;
        else avp_layer_resolve_rows(&jobs[i]);
    }
#endif
    avp_layer_resolve_rows(&jobs[0]);
#if !defined(_WIN32)
    for(unsigned i=1u;i<n;i++)if(started[i])
        (void)pthread_join(threads[i],NULL);
#endif
    for(unsigned i=0u;i<n;i++){
        AvpLayerResolveBand *j=&jobs[i];
        if(!j->ok)return 0;
        *active+=j->active;
        f->masked_pixels+=j->masked;
        f->wall_coverage_pixels+=j->covered;
        for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++){
            const AvpVkSourceBounds *b=&j->bounds[slot];
            AvpVkSourceBounds *dst=&f->visible_bounds[slot];
            if(!j->visible[slot])continue;
            if(!visible[slot]){
                *dst=*b;
            }else{
                if(b->x0<dst->x0)dst->x0=b->x0;
                if(b->y0<dst->y0)dst->y0=b->y0;
                if(b->x1>dst->x1)dst->x1=b->x1;
                if(b->y1>dst->y1)dst->y1=b->y1;
            }
            visible[slot]=1u;
        }
    }
    return 1;
}
void avp_vk_layered_frame_dispose(AvpVkLayeredFrame *f)
{
    if(!f)return;
    avp_vk_flat_projection_dispose(&f->geometry);
    avp_vk_flat_projection_dispose(&f->flats);
    avp_vk_wall_layers_dispose(&f->layers);
    for(uint32_t i=0;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)free(f->owned_words[i]);
    free(f->cry_lut);
    memset(f,0,sizeof(*f));
}
int avp_vk_layered_frame_validate(const AvpVkLayeredFrame *f)
{
    uint32_t active=0u,seen[AVP_WORLD_SCENE_IMAGE_SLOTS]={0},unique=0u;
    if(!f||!f->geometry.data||!f->flats.data||!f->layers.column_start||
       !f->cry_lut||f->geometry.bytes!=f->flats.bytes||
       f->geometry.bytes!=AVP_VK_FLAT_HEADER_BYTES+
            (size_t)f->layers.width*f->layers.height*16u||
       f->geometry.bytes>AVP_VK_FLAT_MAX_BYTES||
       get32(f->geometry.data)!=AVP_VK_FLAT_MAGIC||
       get32(f->geometry.data+4u)!=AVP_VK_FLAT_VERSION||
       get32(f->geometry.data+8u)!=f->layers.width||
       get32(f->geometry.data+12u)!=f->layers.height||
       get32(f->geometry.data+16u)!=16u||
       get32(f->geometry.data+20u)!=32u||
       get32(f->geometry.data+28u)!=0u)return 0;
    for(uint32_t y=0u;y<f->layers.height;y++){
        for(uint32_t x=0u;x<f->layers.width;x++){
            AvpVkFlatTexel ref;
            const uint8_t *p=pixel_at(&f->geometry,f->layers.width,x,y);
            if(!replay_pixel(f,x,y,&ref,NULL,NULL)||
               get32(p)!=ref.slot||get32(p+4u)!=ref.source_x||
               get32(p+8u)!=ref.source_y||
               (int32_t)get32(p+12u)!=ref.cry_shade)return 0;
            if(ref.slot!=AVP_VK_FLAT_NO_IMAGE){active++;
                if(!seen[ref.slot]){seen[ref.slot]=1u;unique++;}}
        }
    }
    if(active!=f->geometry.source_pixels || unique!=f->geometry.source_slots ||
       active!=get32(f->geometry.data+24u)||
       unique!=f->slots.used_count||
       f->slots.active_columns!=unique)return 0;
    uint32_t index=0u;
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++){
        if(!seen[i]){
            if(f->slots.slots[i].used)return 0;
        }else if(index>=f->slots.used_count || f->slots.order[index++]!=i ||
                !f->slots.slots[i].used || !f->images[i] ||
                f->slots.slots[i].width!=f->pixels[i].width ||
                f->slots.slots[i].height!=f->pixels[i].height ||
                f->slots.slots[i].encoding!=f->pixels[i].encoding ||
                f->upload_images[i].alpha!=NULL)return 0;
    }
    return 1;
}
/* R377: the builder has just resolved each source pixel (with exact far-to-near
 * alpha validation) into disjoint rows, so a second *serial* replay over every
 * pixel is unnecessary for the ordinary gameplay path. Validate the builder's
 * complete slot/geometry metadata in O(slots) instead. The public exhaustive
 * validator remains available for fixture/diagnostic testing. Gameplay also
 * independently checks every reconstructed source pixel against the Software
 * world and proves the first GPU-rendered frame via readback. */
static int avp_vk_layered_builder_metadata_valid(const AvpVkLayeredFrame *f,
    const uint8_t visible[AVP_WORLD_SCENE_IMAGE_SLOTS],
    uint32_t active,uint32_t unique)
{
    uint32_t index=0u;
    if(!f||!visible||!f->geometry.data||!f->flats.data||
       !f->layers.column_start||!f->cry_lut||!f->layers.width||
       !f->layers.height||f->geometry.bytes!=f->flats.bytes||
       (uint64_t)f->layers.width*f->layers.height>
           (SIZE_MAX-AVP_VK_FLAT_HEADER_BYTES)/16u||
       f->geometry.bytes!=AVP_VK_FLAT_HEADER_BYTES+
            (size_t)f->layers.width*f->layers.height*16u||
       f->geometry.bytes>AVP_VK_FLAT_MAX_BYTES||
       get32(f->geometry.data)!=AVP_VK_FLAT_MAGIC||
       get32(f->geometry.data+4u)!=AVP_VK_FLAT_VERSION||
       get32(f->geometry.data+8u)!=f->layers.width||
       get32(f->geometry.data+12u)!=f->layers.height||
       get32(f->geometry.data+16u)!=16u||
       get32(f->geometry.data+20u)!=32u||
       get32(f->geometry.data+24u)!=active||
       get32(f->geometry.data+28u)!=0u||
       active!=f->geometry.source_pixels||
       unique!=f->geometry.source_slots||
       unique!=f->slots.used_count||
       f->slots.active_columns!=unique)return 0;
    for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++){
        if(!visible[slot]){
            if(f->slots.slots[slot].used)return 0;
            continue;
        }
        if(index>=unique||f->slots.order[index++]!=slot||
           !f->slots.slots[slot].used||!f->images[slot]||
           !f->pixels[slot].words||
           f->slots.slots[slot].width!=f->pixels[slot].width||
           f->slots.slots[slot].height!=f->pixels[slot].height||
           f->slots.slots[slot].encoding!=f->pixels[slot].encoding||
           f->upload_images[slot].alpha!=NULL||
           f->visible_bounds[slot].x1>f->layers.width||
           f->visible_bounds[slot].y1>f->layers.height||
           f->visible_bounds[slot].x0>=f->visible_bounds[slot].x1||
           f->visible_bounds[slot].y0>=f->visible_bounds[slot].y1)return 0;
    }
    return index==unique;
}
int avp_vk_layered_frame_build(const AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,AvpVkLayeredFrame *out)
{
    AvpVkLayeredFrame *f=NULL;
    AvpWorldGpuStaging staging={0};
    uint8_t used[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint8_t visible[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint32_t active=0u,unique=0u;
    uint64_t total=0u;
    int ok=0;
    if(!scene||!state||!out||out->geometry.data||out->flats.data||
       out->layers.column_start||out->slots.used_count||out->cry_lut)return 0;
    f=(AvpVkLayeredFrame*)calloc(1u,sizeof(*f));
    if(!f)return 0;
    if(!avp_vk_flat_projection_build(scene,&f->flats) ||
       !avp_vk_wall_layers_build(scene,&f->layers))goto done;
    if(f->flats.bytes>AVP_VK_FLAT_MAX_BYTES)goto done;
    f->geometry.data=(uint8_t*)malloc(f->flats.bytes);
    if(!f->geometry.data)goto done;
    /* R378: the row-band resolver writes every 16-byte output record.
     * Only the 32-byte AVFL header must be inherited from the flat source.
     * Do not copy another 31.6 MiB before immediately overwriting it. */
    memcpy(f->geometry.data,f->flats.data,AVP_VK_FLAT_HEADER_BYTES);
    f->geometry.bytes=f->flats.bytes;
    f->geometry.pixels=f->flats.pixels;
    for(uint32_t x=0u;x<f->layers.width;x++)
        for(uint32_t i=f->layers.column_start[x];
            i<f->layers.column_start[x+1u];i++)
            used[f->layers.spans[i].source_slot]=1u;
    for(uint32_t i=0u;i<f->flats.pixels;i++){
        const uint8_t *p=f->flats.data+AVP_VK_FLAT_HEADER_BYTES+(size_t)i*16u;
        uint32_t slot=get32(p);
        if(slot!=AVP_VK_FLAT_NO_IMAGE)used[slot]=1u;
    }
    for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++){
        const AvpGpuRgbImage *image;
        size_t n;
        if(!used[slot])continue;
        image=avp_world_scene_source(state,slot);
        if(!image||!image->rgb||image->width<=0||image->height<=0||
           image->width>8192||image->height>8192||
           scene->image[slot].width!=(uint32_t)image->width||
           scene->image[slot].height!=(uint32_t)image->height||
           scene->image[slot].encoding!=(image->cry16 ?
              AVP_WORLD_IMAGE_CRY16_ALPHA8:AVP_WORLD_IMAGE_RGB8_ALPHA8))goto done;
        n=(size_t)image->width*(size_t)image->height*4u;
        if(n>64u*1024u*1024u||total+n>512ull*1024u*1024u||
           !avp_world_gpu_stage_image(&staging,image,64u*1024u*1024u) ||
           staging.byte_count!=n)goto done;
        total+=n;
        /* The staging allocation ALREADY contains the exact packed source.
         * Transfer ownership to the immutable frame instead of allocating
         * and memcpy'ing a second full texture. The frame's destructor frees
         * the allocation, and later staging calls start with a fresh buffer. */
        f->owned_words[slot]=staging.words;
        f->pixels[slot]=(AvpVkWallPixelSource){f->owned_words[slot],
              staging.width,staging.height,staging.encoding};
        f->images[slot]=image;
        staging.words=NULL;
        staging.word_capacity=0u;
        avp_world_gpu_staging_invalidate(&staging);
    }
    f->cry_lut=(uint32_t*)malloc(65536u*sizeof(uint32_t));
    if(!f->cry_lut||!avp_world_gpu_build_cry_lut(f->cry_lut,65536u))goto done;
    if(!avp_layer_resolve_all(f,visible,&active))goto done;
    for(uint32_t slot=0u;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++){
        if(!visible[slot])continue;
        const AvpVkWallPixelSource *src=&f->pixels[slot];
        f->slots.order[unique++]=slot;
        f->slots.slots[slot]=(AvpVkWallSlot){1u,1u,src->width,src->height,
                                             src->encoding};
        f->upload_images[slot]=*f->images[slot];
        /* Coverage is already RESOLVED in the per-pixel AVFL tape. GPU
         * samplers must not discard masked texels that are never referenced.
         * Only this separate upload view clears the alpha pointer. Original
         * scene source and independent oracle retain their exact alpha. */
        f->upload_images[slot].alpha=NULL;
    }
    f->slots.used_count=unique;
    f->slots.active_columns=unique;
    f->geometry.source_pixels=active;
    f->geometry.source_slots=unique;
    put32(f->geometry.data+24u,active);
    if(!avp_vk_layered_builder_metadata_valid(f,visible,active,unique))goto done;
    /* Explicit developer audit: preserve the old exhaustive independently
     * replayed builder check for A/B testing without charging it to every
     * normal Hybrid gameplay frame. This only ADDS validation. */
    {const char *audit=getenv("AVP_HYBRID_FULL_VALIDATE");
     if(audit&&strcmp(audit,"1")==0 &&
        !avp_vk_layered_frame_validate(f))goto done;}
    f->validated_build=1u; /* no writes to packet after this point */
    *out=*f;memset(f,0,sizeof(*f));ok=1;
done:
    avp_world_gpu_staging_dispose(&staging);
    avp_vk_layered_frame_dispose(f);free(f);
    return ok;
}
/* The independent layer/alpha replay happened during frame construction,
 * and the packet remains immutable until it is disposed. Do not run that
 * expensive replay again inside the Vulkan submitter. Standalone diagnostics
 * without the builder certificate still use the original full validator. */
int avp_vk_layered_frame_ready_for_present(const AvpVkLayeredFrame *f)
{
    if(!f)return 0;
    if(!f->validated_build)return avp_vk_layered_frame_validate(f);
    return f->geometry.data&&f->flats.data&&f->layers.column_start&&
        f->cry_lut&&f->slots.used_count&&
        f->geometry.bytes==f->flats.bytes&&
        f->geometry.bytes==AVP_VK_FLAT_HEADER_BYTES+
            (size_t)f->layers.width*f->layers.height*16u&&
        f->geometry.bytes<=AVP_VK_FLAT_MAX_BYTES&&
        get32(f->geometry.data)==AVP_VK_FLAT_MAGIC&&
        get32(f->geometry.data+4u)==AVP_VK_FLAT_VERSION&&
        get32(f->geometry.data+8u)==f->layers.width&&
        get32(f->geometry.data+12u)==f->layers.height&&
        get32(f->geometry.data+16u)==16u&&
        get32(f->geometry.data+20u)==32u&&
        get32(f->geometry.data+28u)==0u;
}

/* Run this only on a freshly builder-validated packet; it uses the validated
 * original-source reference rather than replaying all occluded wall spans a
 * third time. The separate oracle_make/oracle_matches_world remain independent
 * and the GPU's first complete frame still undergoes exact readback parity. */
/* Integer source->destination bounding: floor(destination*source/region)
 * must stay inside the native interval [x0,x1). Its inverse pixel range is
 * [ceil(x0*region/source),ceil(x1*region/source)). This remains conservative
 * for odd display resolutions, letterboxing, and downsampling. */
int avp_vk_layered_frame_slot_scissor(const AvpVkLayeredFrame *f,
    uint32_t slot,uint32_t region_x,uint32_t region_y,
    uint32_t region_width,uint32_t region_height,
    uint32_t *x,uint32_t *y,uint32_t *width,uint32_t *height)
{
    AvpVkSourceBounds b;
    uint64_t sx0,sy0,sx1,sy1;
    uint32_t sw,sh;
    if(!f||!x||!y||!width||!height||
       slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||!region_width||!region_height||
       region_width>UINT32_MAX-region_x||region_height>UINT32_MAX-region_y)
        return 0;
    if(!f->validated_build){
        *x=region_x;*y=region_y;
        *width=region_width;*height=region_height;
        return 1;
    }
    sw=f->layers.width;sh=f->layers.height;
    b=f->visible_bounds[slot];
    if(!sw||!sh||b.x0>=b.x1||b.y0>=b.y1||
       b.x1>sw||b.y1>sh)return 0;
    sx0=((uint64_t)b.x0*region_width+sw-1u)/sw;
    sx1=((uint64_t)b.x1*region_width+sw-1u)/sw;
    sy0=((uint64_t)b.y0*region_height+sh-1u)/sh;
    sy1=((uint64_t)b.y1*region_height+sh-1u)/sh;
    if(sx0>=sx1||sy0>=sy1||sx1>region_width||sy1>region_height){
        /* Slot is invisible at this downsampled output scale. */
        *x=region_x;*y=region_y;*width=0u;*height=0u;
        return 1;
    }
    *x=region_x+(uint32_t)sx0;*y=region_y+(uint32_t)sy0;
    *width=(uint32_t)(sx1-sx0);*height=(uint32_t)(sy1-sy0);
    return 1;
}

/* R379: Independent exact-color comparison against the original Software
 * raster remains mandatory. At 1080p it used to replay ~2 million CRY/RGB
 * samples on the main thread AFTER the four-worker geometry build. Split
 * only this immutable scan into disjoint row bands; no pixel is skipped,
 * and all bands are joined before the final mismatch count is returned. */
typedef struct AvpLayerWorldCompareBand {
    const AvpVkLayeredFrame *frame;
    const uint32_t *world;
    uint32_t pitch,width,y0,y1;
    uint32_t mismatches;
    int ok;
} AvpLayerWorldCompareBand;
static void avp_layer_world_compare_band(AvpLayerWorldCompareBand *job)
{
    const AvpVkLayeredFrame *f=job->frame;
    uint32_t errors=0u;
    job->ok=0;
    for(uint32_t y=job->y0;y<job->y1;y++){
        const uint32_t *row=job->world+(size_t)y*job->pitch;
        const uint8_t *p=f->geometry.data+AVP_VK_FLAT_HEADER_BYTES+
            (size_t)y*job->width*16u;
        for(uint32_t x=0u;x<job->width;x++,p+=16u){
            uint32_t rgb=0u;uint8_t alpha=0u;
            if(!read_ref(f,p,&rgb,&alpha)||
               (get32(p)!=AVP_VK_FLAT_NO_IMAGE&&alpha!=255u))return;
            if((row[x]&0xffffffu)!=rgb)errors++;
        }
    }
    job->mismatches=errors;
    job->ok=1;
}
#if !defined(_WIN32)
static void *avp_layer_world_compare_thread(void *opaque)
{
    avp_layer_world_compare_band((AvpLayerWorldCompareBand*)opaque);
    return NULL;
}
#endif
int avp_vk_layered_frame_validated_tape_matches_world(const AvpVkLayeredFrame *f,
    const uint32_t *world,uint32_t pitch,uint32_t width,uint32_t height,
    uint32_t *mismatches)
{
    AvpLayerWorldCompareBand jobs[4]={{0}};
    uint32_t errors=0u;
    unsigned n=1u;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!f||!f->validated_build||!mismatches||!world||
       !f->geometry.data||!f->cry_lut||!width||!height||
       width!=f->layers.width||height!=f->layers.height||pitch<width||
       width>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES/4u||
       height>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES/4u/width||
       (size_t)pitch>SIZE_MAX/sizeof(uint32_t)/height)return 0;
#if !defined(_WIN32)
    pthread_t threads[4];
    int started[4]={0};
    if((uint64_t)width*height>=262144u)n=4u;
    {const char *override=getenv("AVP_HYBRID_SOURCE_THREADS");
     if(override&&strcmp(override,"1")==0)n=1u;
     else if(override&&strcmp(override,"4")==0&&height>=4u)n=4u;}
    if(height<n)n=1u;
#endif
    for(unsigned i=0u;i<n;i++){
        jobs[i].frame=f;
        jobs[i].world=world;
        jobs[i].pitch=pitch;
        jobs[i].width=width;
        jobs[i].y0=(uint32_t)((uint64_t)i*height/n);
        jobs[i].y1=(uint32_t)((uint64_t)(i+1u)*height/n);
    }
#if !defined(_WIN32)
    for(unsigned i=1u;i<n;i++){
        if(pthread_create(&threads[i],NULL,avp_layer_world_compare_thread,
                          &jobs[i])==0)started[i]=1;
        else avp_layer_world_compare_band(&jobs[i]);
    }
#endif
    avp_layer_world_compare_band(&jobs[0]);
#if !defined(_WIN32)
    for(unsigned i=1u;i<n;i++)if(started[i])
        (void)pthread_join(threads[i],NULL);
#endif
    for(unsigned i=0u;i<n;i++){
        if(!jobs[i].ok)return 0; /* fail-closed; never trust partial parity */
        errors+=jobs[i].mismatches;
    }
    *mismatches=errors;
    return errors==0u;
}
int avp_vk_layered_frame_oracle_make(const AvpVkLayeredFrame *f,
    uint32_t format,uint8_t *dst,size_t bytes)
{
    uint8_t *temp=NULL;
    if(!f||!dst||!f->flats.data||!f->layers.column_start||
       (format!=AVP_VK_WALL_PIXEL_RGBA8&&format!=AVP_VK_WALL_PIXEL_BGRA8)||
       (uint64_t)f->layers.width*f->layers.height*4u!=bytes||
       bytes>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES)return 0;
    temp=(uint8_t*)malloc(bytes);
    if(!temp)return 0;
    for(uint32_t y=0u;y<f->layers.height;y++){
        for(uint32_t x=0u;x<f->layers.width;x++){
            AvpVkFlatTexel ref;
            uint32_t rgb=0u;uint8_t alpha=0u;
            size_t at=((size_t)y*f->layers.width+x)*4u;
            /* Derive from original independently captured source layers,
             * NEVER from the GPU stream and never from Software RGB pixels. */
            if(!replay_pixel(f,x,y,&ref,NULL,NULL))goto fail;
            if(ref.slot!=AVP_VK_FLAT_NO_IMAGE &&
               (!source_ref(f,ref.slot,ref.source_x,ref.source_y,
                            ref.cry_shade,&rgb,&alpha)||alpha!=255u))goto fail;
            temp[at]=(uint8_t)(format==AVP_VK_WALL_PIXEL_BGRA8 ? rgb : rgb>>16u);
            temp[at+1u]=(uint8_t)(rgb>>8u);
            temp[at+2u]=(uint8_t)(format==AVP_VK_WALL_PIXEL_BGRA8 ? rgb>>16u : rgb);
            temp[at+3u]=255u;
        }
    }
    memcpy(dst,temp,bytes);free(temp);return 1;
fail:
    free(temp);return 0;
}
int avp_vk_layered_frame_oracle_matches_world(const AvpVkLayeredFrame *f,
    const uint32_t *software_world,uint32_t pitch,uint32_t width,uint32_t height,
    uint32_t *mismatches)
{
    uint32_t errors=0u;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!f||!mismatches||!software_world||!f->flats.data||
       !f->layers.column_start||!f->cry_lut||!width||!height||
       width!=f->layers.width||height!=f->layers.height||pitch<width||
       width>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES/4u||
       height>AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES/4u/width||
       (size_t)pitch>SIZE_MAX/sizeof(uint32_t)/height)return 0;
    for(uint32_t y=0u;y<height;y++){
        const uint32_t *row=software_world+(size_t)y*pitch;
        for(uint32_t x=0u;x<width;x++){
            AvpVkFlatTexel ref;
            uint32_t rgb=0u;
            uint8_t alpha=0u;
            /* Preserve original independent wall/portal alpha and depth
             * replay. Do not use the already-resolved GPU geometry. */
            if(!replay_pixel(f,x,y,&ref,NULL,NULL))return 0;
            if(ref.slot!=AVP_VK_FLAT_NO_IMAGE &&
               (!source_ref(f,ref.slot,ref.source_x,ref.source_y,
                            ref.cry_shade,&rgb,&alpha)||alpha!=255u))return 0;
            if((row[x]&0x00ffffffu)!=rgb)errors++;
        }
    }
    *mismatches=errors;
    return errors==0u;
}
int avp_vk_layered_frame_compare(const AvpVkLayeredFrame *f,uint32_t format,
    const uint8_t *gpu,size_t bytes,uint32_t *mismatches)
{
    uint8_t *oracle=NULL;uint32_t errors=0u;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!mismatches||!gpu||!f||!f->layers.width||
       (uint64_t)f->layers.width*f->layers.height*4u!=bytes)return 0;
    oracle=(uint8_t*)malloc(bytes);
    if(!oracle)return 0;
    if(!avp_vk_layered_frame_oracle_make(f,format,oracle,bytes)){
        free(oracle);return 0;
    }
    for(size_t i=0;i<bytes;i+=4u)
        if(memcmp(gpu+i,oracle+i,4u)!=0)errors++;
    free(oracle);
    *mismatches=errors;
    return errors==0u;
}
