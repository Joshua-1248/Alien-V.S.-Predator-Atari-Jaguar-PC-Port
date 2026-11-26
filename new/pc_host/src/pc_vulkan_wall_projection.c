/* RE46 R336: conservative source-derived GPU wall projection records.
 * This is geometry preparation only; Software drawing is never invoked. */
#include "pc/pc_vulkan_wall_projection.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/hires_wall_sample.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void put32(uint8_t *p,uint32_t n)
{
    p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);
    p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
void avp_vk_wall_projection_invalidate(AvpVkWallProjectionStream *s)
{
    if(s)s->bytes=0u;
}
void avp_vk_wall_projection_destroy(AvpVkWallProjectionStream *s)
{
    if(!s)return;
    free(s->data);memset(s,0,sizeof(*s));
}
int avp_vk_wall_projection_sample_y(int y,int base_top,
                                    int projected_height,int texture_height)
{
    if(projected_height<=0 || texture_height<=0 || texture_height>8192)return -1;
    if(texture_height>128){
        /* Equivalent to the original high-res source-row phase for plain
         * walls away from floating-point accumulation boundaries. */
        return avp_gpu_hires_wall_y(y-base_top,projected_height,0,
                                    (unsigned)texture_height);
    }
    return avp_gpu_maze2_host_source_row(y-base_top,projected_height,0)*
           texture_height/128;
}
int avp_vk_wall_projection_pack(AvpVkWallProjectionStream *dst,
                                const AvpWorldScenePacket *p)
{
    uint8_t *b;
    size_t needed;
    uint32_t active=0;
    if(!dst)return 0;
    dst->bytes=0u;
    if(!p || !p->maze || !p->map_width || !p->map_height ||
       !p->columns || !p->hits || !p->width || !p->height ||
       p->width>AVP_WORLD_SCENE_MAX_WIDTH ||
       p->height>AVP_WORLD_SCENE_MAX_HEIGHT ||
       !p->hit_capacity_per_column ||
       p->hit_capacity_per_column>AVP_WORLD_SCENE_MAX_HITS ||
       p->image_slot_count!=AVP_WORLD_SCENE_IMAGE_SLOTS ||
       !isfinite(p->editor_pitch) || fabs(p->editor_pitch)>2.0)return 0;
    needed=AVP_VK_WALL_PROJECTION_HEADER_BYTES+
           (size_t)p->width*AVP_VK_WALL_PROJECTION_COLUMN_BYTES;
    if(needed>1024u*1024u)return 0;
    /* Validate the whole frame before allocating or modifying it. */
    for(uint32_t x=0;x<p->width;x++) {
        const AvpWorldRayColumn *c=&p->columns[x];
        if(!c->ready || c->overflow || c->count>1u ||
           !isfinite(c->wall_depth) || c->wall_depth<0.0)return 0;
        if(c->count){
            const AvpWorldRayHit *h=&p->hits[(size_t)x*p->hit_capacity_per_column];
            uint32_t type=(h->wall>>3u)&15u,slot;
            const double dist=h->distance<0.001?0.001:h->distance;
            if(!h->is_final || h->wall>255u || h->open>255u ||
               !isfinite(h->distance) || h->distance<0.0 ||
               !isfinite(h->texture_fraction) ||
               h->texture_fraction<0.0 || h->texture_fraction>1.0 ||
               h->vertical>1u || (type!=0u && type!=1u && type!=2u && type!=12u) ||
               h->clip_lo<0 || h->clip_hi>(int32_t)p->height ||
               h->clip_lo>h->clip_hi ||
               avp_gpu_maze2_project_wall_height(dist,p->height)>
                   (int)AVP_VK_WALL_PROJECTION_MAX_HEIGHT)return 0;
            slot=p->wall_image[h->wall];
            if(slot>=AVP_WORLD_SCENE_IMAGE_SLOTS ||
               !p->image[slot].width || !p->image[slot].height ||
               p->image[slot].width>8192u || p->image[slot].height>8192u ||
               (p->image[slot].encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
                p->image[slot].encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8))return 0;
        }
    }
    if(dst->capacity<needed){
        uint8_t *alloc=realloc(dst->data,needed);
        if(!alloc)return 0;
        dst->data=alloc;dst->capacity=needed;
    }
    b=dst->data;memset(b,0,needed);
    put32(b,AVP_VK_WALL_PROJECTION_MAGIC);
    put32(b+4,AVP_VK_WALL_PROJECTION_VERSION);
    put32(b+8,p->width);put32(b+12,p->height);
    put32(b+16,AVP_VK_WALL_PROJECTION_COLUMN_BYTES);
    put32(b+20,AVP_VK_WALL_PROJECTION_HEADER_BYTES);
    for(uint32_t x=0;x<p->width;x++) {
        const AvpWorldRayColumn *c=&p->columns[x];
        uint8_t *r=b+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                     (size_t)x*AVP_VK_WALL_PROJECTION_COLUMN_BYTES;
        if(c->count){
            const AvpWorldRayHit *h=&p->hits[(size_t)x*p->hit_capacity_per_column];
            uint32_t slot=p->wall_image[h->wall];
            const AvpWorldImageMeta *img=&p->image[slot];
            double depth=h->distance<0.001?0.001:h->distance;
            int wall_h=avp_gpu_maze2_project_wall_height(depth,p->height),
                y0=0,y1=0,tx=(int)(h->texture_fraction*128.0);
            if(!avp_gpu_maze2_centered_wall_bounds(p->height,wall_h,&y0,&y1)){
                dst->bytes=0u;return 0;
            }
            if(p->editor_world_only){
                int pitch_shift=(int)(p->editor_pitch*(double)p->height*0.52);
                y0+=pitch_shift;y1+=pitch_shift;
            }
            wall_h=y1-y0;
            if(y0<-(int)AVP_VK_WALL_PROJECTION_BASE_BIAS ||
               y0>=(int)AVP_VK_WALL_PROJECTION_BASE_BIAS)return 0;
            int lo=y0<h->clip_lo?h->clip_lo:y0,
                hi=y1>h->clip_hi?h->clip_hi:y1;
            if(lo<0)lo=0;
            if(hi>(int)p->height)hi=(int)p->height;
            if(tx<0)tx=0;
            if(tx>127)tx=127;
            if(h->wall&0x80u)tx=127-tx;
            if(img->width>128u)
                tx=avp_gpu_hires_wall_x(tx,h->texture_fraction,
                                       (h->wall&0x80u)!=0,0,img->width);
            else tx=tx*(int)img->width/128;
            if(tx>=(int)img->width)tx=(int)img->width-1;
            if(lo<hi){
                put32(r,1u);put32(r+4,slot);put32(r+8,h->wall);
                put32(r+12,(uint32_t)lo);put32(r+16,(uint32_t)hi);
                put32(r+20,(uint32_t)(y0+(int)AVP_VK_WALL_PROJECTION_BASE_BIAS));
                put32(r+24,(uint32_t)wall_h);
                put32(r+28,(uint32_t)tx);
                put32(r+32,img->width);put32(r+36,img->height);
                put32(r+40,img->encoding);
                active++;
            }
        }
    }
    put32(b+24,active);
    dst->bytes=needed;
    return 1;
}
/* The R345 row-map tail uses word indices, not native pointers/structs.
 * All offsets are in the same std430 uint[] buffer as the wall records. */
static uint32_t source_row_integer(const uint8_t *r,int screen_y)
{
    const int top=(int)get32(r+20u)-(int)AVP_VK_WALL_PROJECTION_BASE_BIAS;
    const int projected=(int)get32(r+24u);
    const int height=(int)get32(r+36u);
    const int64_t num=((int64_t)(screen_y-top)*2+1)*height;
    int64_t row;
    if(height>128) row=num/((int64_t)projected*2);
    else {
        int64_t native=((int64_t)(screen_y-top)*2+1)*128/
                       ((int64_t)projected*2);
        if(native<0)native=0;
        if(native>127)native=127;
        row=native*height/128;
    }
    if(row<0)row=0;
    if(row>=height)row=height-1;
    return (uint32_t)row;
}
static int mapped_source_row(const uint8_t *b,const uint8_t *r,
                             uint32_t y,uint32_t *source_row)
{
    const uint32_t offset=get32(r+48u),top=get32(r+12u);
    const uint32_t i=y-top;
    const uint32_t word=get32(b+4u*(size_t)(offset+i/2u));
    if(!offset || !source_row)return 0;
    *source_row=(word>>((i&1u)*16u))&65535u;
    return 1;
}
int avp_vk_wall_projection_validate(const uint8_t *b,size_t bytes)
{
    uint32_t w,h,active=0u,stride,version;
    uint32_t next_word=0u;
    if(!b || bytes<AVP_VK_WALL_PROJECTION_HEADER_BYTES ||
       get32(b)!=AVP_VK_WALL_PROJECTION_MAGIC ||
       ((version=get32(b+4))!=AVP_VK_WALL_PROJECTION_VERSION &&
        version!=AVP_VK_WALL_PROJECTION_LIT_VERSION &&
        version!=AVP_VK_WALL_PROJECTION_EXACT_VERSION) ||
       !(w=get32(b+8)) || w>AVP_WORLD_SCENE_MAX_WIDTH ||
       !(h=get32(b+12)) || h>AVP_WORLD_SCENE_MAX_HEIGHT ||
       ((stride=get32(b+16)) !=
         (version==AVP_VK_WALL_PROJECTION_EXACT_VERSION ?
          AVP_VK_WALL_PROJECTION_EXACT_COLUMN_BYTES :
          version==AVP_VK_WALL_PROJECTION_LIT_VERSION ?
          AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES :
          AVP_VK_WALL_PROJECTION_COLUMN_BYTES)) ||
       get32(b+20)!=AVP_VK_WALL_PROJECTION_HEADER_BYTES ||
       (version!=AVP_VK_WALL_PROJECTION_EXACT_VERSION && get32(b+28)!=0u) ||
       (version==AVP_VK_WALL_PROJECTION_EXACT_VERSION &&
        (bytes>AVP_VK_WALL_PROJECTION_MAX_EXACT_BYTES || (bytes&3u) ||
         get32(b+28)!=8u+13u*w)) ||
       bytes<AVP_VK_WALL_PROJECTION_HEADER_BYTES+(size_t)w*stride ||
       (version!=AVP_VK_WALL_PROJECTION_EXACT_VERSION &&
        bytes!=AVP_VK_WALL_PROJECTION_HEADER_BYTES+(size_t)w*stride))return 0;
    next_word=8u+(stride/4u)*w;
    for(uint32_t x=0;x<w;x++){
        const uint8_t *r=b+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                         (size_t)x*stride;
        if(get32(r)>1u)return 0;
        if(get32(r)==1u){
            uint32_t slot=get32(r+4),wall=get32(r+8),
                     top=get32(r+12),bottom=get32(r+16),
                     texx=get32(r+28),tw=get32(r+32),th=get32(r+36),
                     encoding=get32(r+40);
            int32_t base=(int32_t)get32(r+20)-(int32_t)AVP_VK_WALL_PROJECTION_BASE_BIAS,
                    projected=(int32_t)get32(r+24);
            uint32_t t=(wall>>3u)&15u;
            if(slot>=AVP_WORLD_SCENE_IMAGE_SLOTS || wall>255u ||
               (t!=0u && t!=1u && t!=2u && t!=12u) ||
               top>=bottom || bottom>h || !projected ||
               projected>(int)AVP_VK_WALL_PROJECTION_MAX_HEIGHT ||
               get32(r+20)>65535u ||
               base>INT32_MAX-projected || texx>=tw || !tw || !th ||
               (int64_t)top<(int64_t)base ||
               (int64_t)bottom>(int64_t)base+projected ||
               tw>8192u || th>8192u ||
               (encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
                encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8))return 0;
            if(version>=AVP_VK_WALL_PROJECTION_LIT_VERSION){
                const int32_t shade=(int32_t)get32(r+44u);
                if(shade < -255 || shade > 255 ||
                   (encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8 && shade!=0))
                    return 0;
            }
            if(version==AVP_VK_WALL_PROJECTION_EXACT_VERSION){
                uint32_t offset=get32(r+48u);
                if(offset){
                    const uint32_t count=bottom-top;
                    const uint32_t words=(count+1u)/2u;
                    if(th<=128u || offset!=next_word ||
                       (uint64_t)offset+words>bytes/4u)return 0;
                    for(uint32_t i=0u;i<count;i++){
                        uint32_t pixel=(get32(b+4u*(size_t)(offset+i/2u))>>
                                         ((i&1u)*16u))&65535u;
                        if(pixel>=th)return 0;
                    }
                    if((count&1u) &&
                       (get32(b+4u*(size_t)(offset+count/2u))>>16u)!=0u)
                        return 0; /* high half must be canonical zero */
                    next_word+=words;
                }
            }
            active++;
        }else for(uint32_t off=4u;off<stride;off+=4u)
            if(get32(r+off)!=0u)return 0;
    }
    return active==get32(b+24) &&
      (version!=AVP_VK_WALL_PROJECTION_EXACT_VERSION ||
       next_word==bytes/4u);
}

int avp_vk_wall_projection_lookup_row(const uint8_t *wire,size_t bytes,
                                      uint32_t column,uint32_t screen_row,
                                      int *source_row)
{
    const uint32_t stride=wire && bytes>=32u ? get32(wire+16u) : 0u;
    const uint32_t version=wire && bytes>=32u ? get32(wire+4u) : 0u;
    const uint8_t *r;
    uint32_t mapped;
    if(!source_row || !avp_vk_wall_projection_validate(wire,bytes) ||
       column>=get32(wire+8u))return 0;
    r=wire+32u+(size_t)column*stride;
    if(!get32(r) || screen_row<get32(r+12u) ||
       screen_row>=get32(r+16u))return 0;
    if(version==AVP_VK_WALL_PROJECTION_EXACT_VERSION && get32(r+48u)){
        if(!mapped_source_row(wire,r,screen_row,&mapped))return 0;
        *source_row=(int)mapped;
    }else *source_row=(int)source_row_integer(r,(int)screen_row);
    return 1;
}

/* RE47 R344: the Software renderer deliberately advances sample_y in
 * double precision, one visible pixel at a time. The Vulkan shader's exact
 * integer fraction may land on the *other* side of a texel boundary when
 * that repeated addition rounds slightly down. Do not certify a frame as
 * pixel-identical until that difference has been resolved. This audit is
 * invoked before image allocation in the optional live-frame bridge. */
int avp_vk_wall_projection_row_parity(const uint8_t *wire,size_t bytes,
                                     uint32_t *mismatches)
{
    uint32_t differences=0u;
    uint32_t width,stride;
    if(!mismatches || !avp_vk_wall_projection_validate(wire,bytes))return 0;
    *mismatches=0u;
    width=get32(wire+8u);
    stride=get32(wire+16u);
    for(uint32_t x=0u;x<width;x++){
        const uint8_t *r=wire+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                         (size_t)x*stride;
        if(!get32(r))continue;
        const int texture_height=(int)get32(r+36u);
        if(texture_height<=128)continue; /* native 128-row integer path */
        const int top=(int)get32(r+12u),bottom=(int)get32(r+16u);
        const int base=(int)get32(r+20u)-(int)AVP_VK_WALL_PROJECTION_BASE_BIAS;
        const int wall_h=(int)get32(r+24u);
        const double step=(double)texture_height/(double)wall_h;
        double source_y=((double)(top-base)+0.5)*step;
        for(int y=top;y<bottom;y++){
            int software=(int)source_y;
            uint32_t gpu=source_row_integer(r,y);
            if(get32(wire+4u)==AVP_VK_WALL_PROJECTION_EXACT_VERSION &&
               get32(r+48u)){
                if(!mapped_source_row(wire,r,(uint32_t)y,&gpu))return 0;
            }
            if(software<0)software=0;
            if(software>=texture_height)software=texture_height-1;
            if(software!=(int)gpu)differences++;
            source_y+=step;
        }
    }
    *mismatches=differences;
    return 1;
}

/* R345: build a compact, exact CPU row map only for high-resolution
 * columns where closed-form integer division differs from Software's
 * initial multiply followed by per-pixel IEEE double addition. All map
 * entries go into the existing geometry SSBO (binding 2), so no additional
 * descriptor lifetime or Vulkan feature is required. */
int avp_vk_wall_projection_upgrade_exact_rows(AvpVkWallProjectionStream *stream,
                                              uint32_t *mapped_columns,
                                              uint32_t *mapped_rows)
{
    uint32_t width,columns=0u,rows=0u;
    uint32_t first_word,word_cursor;
    uint32_t *counts=NULL;
    uint8_t *new_wire=NULL;
    size_t new_bytes;
    if(mapped_columns)*mapped_columns=0u;
    if(mapped_rows)*mapped_rows=0u;
    if(!stream || !stream->data || !stream->bytes ||
       !avp_vk_wall_projection_validate(stream->data,stream->bytes) ||
       get32(stream->data+4u)!=AVP_VK_WALL_PROJECTION_LIT_VERSION)return 0;
    width=get32(stream->data+8u);
    counts=(uint32_t *)calloc(width,sizeof(*counts));
    if(!counts)return 0;
    first_word=8u+13u*width;
    word_cursor=first_word;
    for(uint32_t x=0;x<width;x++){
        const uint8_t *r=stream->data+32u+
                         (size_t)x*AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES;
        const uint32_t th=get32(r+36u);
        if(!get32(r) || th<=128u)continue;
        const int top=(int)get32(r+12u),bottom=(int)get32(r+16u);
        const int base=(int)get32(r+20u)-(int)AVP_VK_WALL_PROJECTION_BASE_BIAS;
        const int projected=(int)get32(r+24u);
        const double step=(double)th/(double)projected;
        double src=((double)(top-base)+0.5)*step;
        int differs=0;
        for(int y=top;y<bottom;y++){
            int row=(int)src;
            if(row<0)row=0;
            if(row>=(int)th)row=(int)th-1;
            if((uint32_t)row!=source_row_integer(r,y))differs=1;
            src+=step;
        }
        if(differs){
            uint32_t visible=(uint32_t)(bottom-top),words=(visible+1u)/2u;
            if((uint64_t)word_cursor+words>UINT32_MAX ||
               ((uint64_t)word_cursor+words)*4u>
                 AVP_VK_WALL_PROJECTION_MAX_EXACT_BYTES)goto fail;
            counts[x]=visible;
            word_cursor+=words;
            columns++;
            rows+=visible;
        }
    }
    if(!columns){free(counts);return 1;}
    new_bytes=(size_t)word_cursor*4u;
    new_wire=(uint8_t *)calloc(1u,new_bytes);
    if(!new_wire)goto fail;
    memcpy(new_wire,stream->data,AVP_VK_WALL_PROJECTION_HEADER_BYTES);
    put32(new_wire+4u,AVP_VK_WALL_PROJECTION_EXACT_VERSION);
    put32(new_wire+16u,AVP_VK_WALL_PROJECTION_EXACT_COLUMN_BYTES);
    put32(new_wire+28u,first_word);
    word_cursor=first_word;
    for(uint32_t x=0;x<width;x++){
        const uint8_t *src=stream->data+32u+
                          (size_t)x*AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES;
        uint8_t *r=new_wire+32u+
                   (size_t)x*AVP_VK_WALL_PROJECTION_EXACT_COLUMN_BYTES;
        memcpy(r,src,AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES);
        if(counts[x]){
            const int top=(int)get32(r+12u),bottom=(int)get32(r+16u);
            const int base=(int)get32(r+20u)-(int)AVP_VK_WALL_PROJECTION_BASE_BIAS;
            const int projected=(int)get32(r+24u),th=(int)get32(r+36u);
            const double step=(double)th/(double)projected;
            double source=((double)(top-base)+0.5)*step;
            put32(r+48u,word_cursor);
            for(int y=top;y<bottom;y++){
                const uint32_t i=(uint32_t)(y-top),off=word_cursor+i/2u;
                int row=(int)source;
                if(row<0)row=0;
                if(row>=th)row=th-1;
                /* Each packed u16 is written exactly once; canonical
                 * zero high half for a final odd pixel is preserved. */
                const uint32_t prior=get32(new_wire+(size_t)off*4u);
                put32(new_wire+(size_t)off*4u,
                      prior|((uint32_t)row<<((i&1u)*16u)));
                source+=step;
            }
            word_cursor+=(counts[x]+1u)/2u;
        }
    }
    if(word_cursor*4u!=new_bytes ||
       !avp_vk_wall_projection_validate(new_wire,new_bytes))goto fail;
    /* A full exact-data check also catches any implementation discrepancy
     * before replacing the previously valid v2 packet. */
    {
        uint32_t check=UINT32_MAX;
        if(!avp_vk_wall_projection_row_parity(new_wire,new_bytes,&check) || check)
            goto fail;
    }
    free(stream->data);
    stream->data=new_wire;
    stream->bytes=stream->capacity=new_bytes;
    if(mapped_columns)*mapped_columns=columns;
    if(mapped_rows)*mapped_rows=rows;
    free(counts);
    return 1;
fail:
    free(new_wire);
    free(counts);
    return 0;
}

/* Independent CPU reference for a 1m plain wall, using the actual Jaguar
 * projection helpers (not decoding the input GPU band stream). The optional
 * diagnostic uploads a 16x16 opaque checkerboard as source image. */
int avp_vk_wall_projection_check_readback(const uint8_t *rgba,size_t bytes,
    uint32_t w,uint32_t h,uint32_t format,uint32_t *mismatches)
{
    uint32_t errors=0;
    int y0=0,y1=0;
    int projected;
    size_t expected;
    if(!rgba || !mismatches || !w || !h || w>8192u || h>8192u ||
       (format!=37u && format!=44u) ||
       (uint64_t)w*h*4u>64u*1024u*1024u)return 0;
    expected=(size_t)w*h*4u;
    if(bytes<expected)return 0;
    *mismatches=0;
    projected=avp_gpu_maze2_project_wall_height(1.0,h);
    if(!avp_gpu_maze2_centered_wall_bounds(h,projected,&y0,&y1))return 0;
    projected=y1-y0;
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
        uint32_t rgb=0u;
        if((int)y>=y0 && (int)y<y1){
            int tx=(int)(x%16u);
            int row=avp_gpu_maze2_host_source_row((int)y-y0,projected,0);
            int ty=row*16/128;
            rgb=((tx^ty)&1)?0x00336699u:0x00bb8855u;
        }
        const uint8_t r=(uint8_t)(rgb>>16u),g=(uint8_t)(rgb>>8u),
                      b=(uint8_t)rgb;
        const uint8_t expect[4]={format==44u?b:r,g,format==44u?r:b,255u};
        size_t off=((size_t)y*w+x)*4u;
        if(memcmp(rgba+off,expect,4u)!=0)++errors;
    }
    *mismatches=errors;
    return errors==0u;
}

/* R337 synthetic multi-texture oracle. Every pair of consecutive 16-column
 * spans references a different GPU source image (slots 5 and 6). A false
 * bind, missing draw or stale fragment must fail the complete pixel check.
 * Expected colors are calculated independently from the GPU geometry stream. */
int avp_vk_wall_projection_check_two_slots(const uint8_t *rgba,size_t bytes,
    uint32_t w,uint32_t h,uint32_t format,uint32_t *mismatches)
{
    uint32_t errors=0u;
    int y0=0,y1=0;
    int projected;
    if(!rgba || !mismatches || !w || !h || w>8192u || h>8192u ||
       (format!=37u && format!=44u) ||
       (uint64_t)w*h*4u>64u*1024u*1024u || bytes<(size_t)w*h*4u)return 0;
    *mismatches=0u;
    projected=avp_gpu_maze2_project_wall_height(1.0,h);
    if(!avp_gpu_maze2_centered_wall_bounds(h,projected,&y0,&y1))return 0;
    projected=y1-y0;
    for(uint32_t y=0;y<h;y++)for(uint32_t x=0;x<w;x++){
        uint32_t rgb=0u;
        if((int)y>=y0 && (int)y<y1){
            int tx=(int)(x%16u);
            int row=avp_gpu_maze2_host_source_row((int)y-y0,projected,0);
            int ty=row*16/128;
            if((x/16u)&1u)
                rgb=((tx^ty)&1)?0x00226644u:0x00ddaa66u;
            else rgb=((tx^ty)&1)?0x00336699u:0x00bb8855u;
        }
        size_t off=((size_t)y*w+x)*4u;
        const uint8_t r=(uint8_t)(rgb>>16u),g=(uint8_t)(rgb>>8u),
                      b=(uint8_t)rgb;
        const uint8_t expected[4]={format==44u?b:r,g,format==44u?r:b,255u};
        if(memcmp(rgba+off,expected,4u)!=0)errors++;
    }
    *mismatches=errors;
    return errors==0u;
}
