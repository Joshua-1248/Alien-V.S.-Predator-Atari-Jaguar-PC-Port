/* R357: exact SourceDrawColumn segment shapes from MAZE2.GAS as transcribed
 * in software_renderer.c::draw_hit_column/draw_wall_seg_shift. No Software
 * frame is created or copied. This is NOT yet a GPU execution backend. */
#include "pc/pc_vulkan_wall_layers.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/hires_wall_sample.h"
#include "jaguar/avp_gpu/hires_window_mask.h"
#include "jaguar/avp_gpu/wall_raster_dda.h"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/texture_filter.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void avp_vk_wall_layers_dispose(AvpVkWallLayers *s)
{
    if(!s)return;
    free(s->column_start);free(s->spans);free(s->rows);
    memset(s,0,sizeof(*s));
}
static int grow_spans(AvpVkWallLayers *s)
{
    uint32_t n=s->span_capacity ? s->span_capacity*2u : 256u;
    if(n>AVP_VK_WALL_LAYER_MAX_SPANS)n=AVP_VK_WALL_LAYER_MAX_SPANS;
    if(s->span_count==n)return 0;
    AvpVkWallLayerSpan *p=(AvpVkWallLayerSpan*)realloc(s->spans,(size_t)n*sizeof(*p));
    if(!p)return 0;
    s->spans=p;s->span_capacity=n;
    return 1;
}
static int grow_rows(AvpVkWallLayers *s,uint32_t need)
{
    uint32_t n=s->row_capacity?s->row_capacity:4096u;
    if(need>AVP_VK_WALL_LAYER_MAX_ROWS)return 0;
    while(n<need){
        if(n>=AVP_VK_WALL_LAYER_MAX_ROWS){n=AVP_VK_WALL_LAYER_MAX_ROWS;break;}
        n=n>AVP_VK_WALL_LAYER_MAX_ROWS/2u ? AVP_VK_WALL_LAYER_MAX_ROWS : n*2u;
    }
    if(n<need)return 0;
    uint16_t *p=(uint16_t*)realloc(s->rows,(size_t)n*sizeof(*p));
    if(!p)return 0;
    s->rows=p;s->row_capacity=n;
    return 1;
}
typedef struct LayerBuilder {
    const AvpWorldScenePacket *scene;
    AvpVkWallLayers *dst;
    uint32_t x;
    const AvpWorldRayHit *hit;
    uint32_t slot;
    uint32_t wall_type;
    uint32_t source_width,source_height;
    uint32_t encoding;
    int base_y0,wall_height,shade;
} LayerBuilder;

/* Segment endpoints are in original unclipped destination pixels. Clipping
 * comes AFTER edge projection, as in draw_wall_seg_shift. */
static int layer_emit(LayerBuilder *b,int first,int last,int raw_x,int shift)
{
    const AvpWorldScenePacket *p=b->scene;
    AvpVkWallLayers *dst=b->dst;
    const AvpWorldRayHit *hit=b->hit;
    int tx,lo=first,hi=last;
    if(lo<hit->clip_lo)lo=hit->clip_lo;
    if(hi>hit->clip_hi)hi=hit->clip_hi;
    if(lo<0)lo=0;
    if(hi>(int)p->height)hi=(int)p->height;
    if(lo>=hi)return 1;
    if(raw_x<0)raw_x=0;
    if(raw_x>127)raw_x=127;
    if(b->source_width>128u){
        tx=avp_gpu_hires_wall_x(raw_x,hit->texture_fraction,
            (hit->wall&0x80u)!=0,b->wall_type==13u,b->source_width);
    }else tx=raw_x*(int)b->source_width/128;
    if(tx<0||tx>=(int)b->source_width ||
       (uint32_t)(hi-lo)>AVP_VK_WALL_LAYER_MAX_ROWS-dst->row_count)return 0;
    if(dst->span_count==dst->span_capacity && !grow_spans(dst))return 0;
    if(dst->row_count+(uint32_t)(hi-lo)>dst->row_capacity &&
       !grow_rows(dst,dst->row_count+(uint32_t)(hi-lo)))return 0;
    AvpVkWallLayerSpan *span=&dst->spans[dst->span_count];
    *span=(AvpVkWallLayerSpan){b->x,b->slot,hit->wall,(uint32_t)tx,
        (uint32_t)lo,(uint32_t)hi,dst->row_count,b->shade,(float)hit->distance};
    int native=b->source_height<=128u;
    if(native){
        AvpWallRowDda dda=avp_wall_row_dda_begin(lo-b->base_y0,b->wall_height);
        for(int y=lo;y<hi;y++){
            int sy;
            if(b->wall_height>=128){
                sy=avp_wall_row_dda_value(&dda)+shift;
                avp_wall_row_dda_advance(&dda);
                if(sy<0)sy=0;
                if(sy>127)sy=127;
            }else sy=avp_gpu_maze2_host_source_row(y-b->base_y0,b->wall_height,shift);
            int ty=sy*(int)b->source_height/128;
            if(ty<0)ty=0;
            if(ty>=(int)b->source_height)ty=(int)b->source_height-1;
            dst->rows[dst->row_count++]=(uint16_t)ty;
        }
    }else{
        /* Do not replace repeated double increments with row*step: high-res
         * texel ties can differ after thousands of original host rows. */
        double step=(double)b->source_height/(double)b->wall_height;
        double sample=((double)(lo-b->base_y0)+0.5)*step+
                       (double)shift*(double)b->source_height/128.0;
        for(int y=lo;y<hi;y++,sample+=step){
            int ty=sample<0.0?0:sample>=(double)b->source_height?
                   (int)b->source_height-1:(int)sample;
            dst->rows[dst->row_count++]=(uint16_t)ty;
        }
    }
    dst->span_count++;
    return 1;
}
static int layer_edge(const LayerBuilder *b,int edge)
{
    return avp_gpu_maze2_host_edge_y(b->base_y0,b->wall_height,edge);
}
static int layer_pair(const LayerBuilder *b,int col,int *top,int *bottom,
                      unsigned *index)
{
    const AvpWorldScenePacket *p=b->scene;
    uint16_t idx=p->wall_window_pair[b->hit->wall];
    if(idx>=13u||!p->window_pair_loaded[idx])return 0;
    if(col<0||col>=128)return 0;
    const uint8_t *raw=p->window_pairs[idx];
    *top=(int)(int8_t)raw[(size_t)col*2u];
    *bottom=(int)(int8_t)raw[(size_t)col*2u+1u];
    if(index)*index=idx;
    return 1;
}
static double layer_subcolumn(double fraction,int reflect,int unreflect)
{
    double f=fraction*128.0;
    f-=floor(f);
    if(reflect)f=1.0-f;
    if(unreflect)f=1.0-f;
    if(f<0.0)f=0.0;
    if(f>1.0)f=1.0;
    return f;
}
static int layer_window_edges(const LayerBuilder *b,unsigned idx,int col,
                              int top,int bottom,int unreflect,
                              int *out_top,int *out_bottom)
{
    *out_top=layer_edge(b,top);
    *out_bottom=layer_edge(b,bottom);
    if(b->source_width>128u && b->scene->height>228u && bottom>top){
        double ft,fb;
        avp_gpu_hires_window_pair(b->scene->window_pairs[idx],col,
             layer_subcolumn(b->hit->texture_fraction,
              (b->hit->wall&0x80u)!=0,unreflect),1,&ft,&fb);
        *out_top=avp_gpu_hires_edge_y(b->base_y0,b->wall_height,ft);
        *out_bottom=avp_gpu_hires_edge_y(b->base_y0,b->wall_height,fb);
    }
    return 1;
}
static int layer_draw_hit(LayerBuilder *b,int frame_only)
{
    const AvpWorldRayHit *h=b->hit;
    uint32_t type=b->wall_type;
    int tx=(int)(h->texture_fraction*128.0),top=b->base_y0,
        bottom=b->base_y0+b->wall_height,reflect=(h->wall&0x80u)!=0;
    if(tx<0)tx=0;
    if(tx>127)tx=127;
    if(reflect)tx=127-tx;
    if(type==3u||type==13u||type==14u||type==15u){
        int wt,wb,mt,mb;unsigned idx;
        if(type==13u && !avp_gpu_maze2_singles_source_col(tx,reflect,&tx))return 1;
        if(layer_pair(b,tx,&wt,&wb,&idx)){
            layer_window_edges(b,idx,tx,wt,wb,type==13u,&mt,&mb);
            return layer_emit(b,top,mt,tx,0)&&layer_emit(b,mb,bottom,tx,0);
        }
        return frame_only?1:layer_emit(b,top,bottom,tx,0);
    }
    if(type>=4u&&type<=6u){
        int wt=0,wb=0,mt,mb,moving=tx,door_tx=tx;
        unsigned idx;
        if(type==6u && !avp_gpu_maze2_centered_door_source_col(tx,h->open,&door_tx))return 1;
        if((type==4u||type==6u)&&layer_pair(b,door_tx==tx?tx:door_tx,&wt,&wb,&idx)){
            layer_window_edges(b,idx,(type==6u?door_tx:tx),wt,wb,0,&mt,&mb);
            if(!layer_emit(b,top,mt,type==6u?door_tx:tx,0)||
               !layer_emit(b,mb,bottom,type==6u?door_tx:tx,0))return 0;
            if(!frame_only && mt<mb && type!=6u &&
               avp_gpu_maze2_centered_door_source_col(tx,h->open,&moving))
                return layer_emit(b,mt,mb,moving,0);
            return 1;
        }
        if(!frame_only && (type==6u||
            avp_gpu_maze2_centered_door_source_col(tx,h->open,&moving)))
            return layer_emit(b,top,bottom,type==6u?door_tx:moving,0);
        return 1;
    }
    if(type>=7u&&type<=11u){
        int open=(int)h->open;
        if(open>127)open=127;
        if(type==7u){
            return layer_emit(b,top,layer_edge(b,64-open),tx,open);
        }
        if(type==8u){
            return layer_emit(b,top,layer_edge(b,-open),tx,open)&&
                   layer_emit(b,layer_edge(b,open),bottom,tx,-open);
        }
        int wt=-64,wb=64,rawt,rawb;unsigned idx;
        if(layer_pair(b,tx,&rawt,&rawb,&idx)){
            wt=rawt;wb=rawb;
            if(wt<-64)wt=-64;
            if(wt>64)wt=64;
            if(wb<-64)wb=-64;
            if(wb>64)wb=64;
            if(wb<wt){int swap=wt;wt=wb;wb=swap;}
        }
        if(type==9u||type==10u){
            int xe=wb-open;if(xe<wt)xe=wt;if(xe>wb)xe=wb;
            return layer_emit(b,top,layer_edge(b,wt),tx,0)&&
                layer_emit(b,layer_edge(b,wt),layer_edge(b,xe),tx,open)&&
                layer_emit(b,layer_edge(b,wb),bottom,tx,0);
        }
        int et=12-open,eb=12+open;
        if(et<wt)et=wt;
        if(et>wb)et=wb;
        if(eb<wt)eb=wt;
        if(eb>wb)eb=wb;
        if(eb<et){int swap=et;et=eb;eb=swap;}
        return layer_emit(b,top,layer_edge(b,wt),tx,0)&&
            layer_emit(b,layer_edge(b,wt),layer_edge(b,et),tx,open)&&
            layer_emit(b,layer_edge(b,eb),layer_edge(b,wb),tx,-open)&&
            layer_emit(b,layer_edge(b,wb),bottom,tx,0);
    }
    return layer_emit(b,top,bottom,tx,0);
}
int avp_vk_wall_layers_build(const AvpWorldScenePacket *scene,
                             AvpVkWallLayers *out)
{
    AvpVkWallLayers candidate={0};
    if(!out||out->column_start||out->spans||out->rows||out->span_count||
       out->row_count||!scene||!avp_world_scene_complete(scene)||
       scene->width>AVP_WORLD_SCENE_MAX_WIDTH||
       scene->height>AVP_WORLD_SCENE_MAX_HEIGHT||
       scene->image_slot_count!=AVP_WORLD_SCENE_IMAGE_SLOTS||
       scene->hit_capacity_per_column>AVP_WORLD_SCENE_MAX_HITS||
       scene->hit_capacity_per_column==0u||
       !isfinite(scene->editor_pitch)||fabs(scene->editor_pitch)>2.0||
       ((scene->filter_mask&(1u<<AVP_FILTER_GEOMETRY)) &&
        (scene->filter_mode || scene->anisotropic_taps>1u)))return 0;
    candidate.width=scene->width;candidate.height=scene->height;
    candidate.column_start=(uint32_t*)calloc((size_t)scene->width+1u,sizeof(uint32_t));
    if(!candidate.column_start)return 0;
    for(uint32_t x=0u;x<scene->width;x++){
        const AvpWorldRayColumn *col=&scene->columns[x];
        candidate.column_start[x]=candidate.span_count;
        if(!col->count)continue;
        /* The raycaster records nearest portal first, final opaque hit last. */
        const AvpWorldRayHit *base=&scene->hits[(size_t)x*scene->hit_capacity_per_column];
        const uint32_t has_final=base[col->count-1u].is_final;
        const uint32_t portal_count=col->count-has_final;
        /* A ray may legitimately leave the visible range after crossing
         * one or more windows: in that case there is NO final wall hit. */
        for(uint32_t order=0u;order<col->count;order++){
            uint32_t hit_index=(has_final && order==0u) ? col->count-1u :
                 portal_count-1u-(order-has_final);
            const AvpWorldRayHit *h=&base[hit_index];
            if(h->wall>255u)goto fail;
            uint32_t type=(h->wall>>3u)&15u;
            uint32_t slot=scene->wall_image[h->wall];
            int wall_h,y0,y1,shade;
            if(!isfinite(h->distance)||h->distance<0.0||
               !isfinite(h->texture_fraction)||h->texture_fraction<0.0||
               h->texture_fraction>1.0||h->wall>255u||h->open>255u||
               h->vertical>1u || h->clip_lo<0||h->clip_hi>(int)scene->height||
               h->clip_lo>h->clip_hi||
               (has_final && order==0u ? !h->is_final:h->is_final)||
               slot>=AVP_WORLD_SCENE_IMAGE_SLOTS ||
               scene->image[slot].encoding==AVP_WORLD_IMAGE_ABSENT||
               (scene->image[slot].encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8&&
                scene->image[slot].encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8)||
               !scene->image[slot].width||!scene->image[slot].height||
               scene->image[slot].width>8192u||scene->image[slot].height>8192u)
                goto fail;
            double depth=h->distance<0.001?0.001:h->distance;
            wall_h=avp_gpu_maze2_project_wall_height(depth,scene->height);
            if(wall_h<=0 || wall_h>32768 ||
               !avp_gpu_maze2_centered_wall_bounds(scene->height,wall_h,&y0,&y1))goto fail;
            if(scene->editor_world_only){
                int shift=(int)(scene->editor_pitch*(double)scene->height*0.52);
                y0+=shift;y1+=shift;
            }
            wall_h=y1-y0;
            shade=avp_host_fog_shade_delta(avp_gpu_retail_wall_intensity_delta(
                   avp_host_fog_distance(depth,(int)scene->fog_distance_mode)),
                   (int)scene->disable_fog);
            if(shade< -255 || shade>255)goto fail;
            LayerBuilder builder={scene,&candidate,x,h,slot,type,
                scene->image[slot].width,scene->image[slot].height,
                scene->image[slot].encoding,y0,wall_h,shade};
            if(!layer_draw_hit(&builder,!(has_final && order==0u)))goto fail;
            if(type>=3u&&type<=15u){
                if(type>=4u&&type<=11u)candidate.doorway_columns++;
                else candidate.window_columns++;
            }
            if(!(has_final && order==0u))candidate.portal_columns++;
        }
    }
    candidate.column_start[scene->width]=candidate.span_count;
    if(!avp_vk_wall_layers_validate(&candidate,scene))goto fail;
    *out=candidate;
    return 1;
fail:
    avp_vk_wall_layers_dispose(&candidate);
    return 0;
}
int avp_vk_wall_layers_validate(const AvpVkWallLayers *s,
                                const AvpWorldScenePacket *scene)
{
    if(!s||!scene||!s->column_start||!scene->width||
       s->width!=scene->width||s->height!=scene->height||
       s->span_count>AVP_VK_WALL_LAYER_MAX_SPANS||
       s->row_count>AVP_VK_WALL_LAYER_MAX_ROWS||
       s->span_count>s->span_capacity||s->row_count>s->row_capacity||
       (s->span_count&&!s->spans)||(s->row_count&&!s->rows)||
       s->column_start[0]!=0u||s->column_start[s->width]!=s->span_count)return 0;
    uint32_t cursor=0u,expected_rows=0u;
    for(uint32_t x=0u;x<s->width;x++){
        uint32_t start=s->column_start[x],end=s->column_start[x+1u];
        if(start!=cursor||start>end||end>s->span_count)return 0;
        for(uint32_t i=start;i<end;i++){
            const AvpVkWallLayerSpan *span=&s->spans[i];
            const AvpWorldImageMeta *image;
            if(span->column!=x||span->source_slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||
               span->wall_code>255u ||span->top>=span->bottom||
               span->bottom>s->height||!isfinite(span->distance)||
               span->distance<0.0f||span->cry_shade< -255||
               span->cry_shade>255)return 0;
            image=&scene->image[span->source_slot];
            if((image->encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
                image->encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8)||
               span->source_slot!=scene->wall_image[span->wall_code] ||
               span->source_x>=image->width||
               span->row_offset!=expected_rows ||
               span->row_offset>s->row_count||
               span->bottom-span->top>s->row_count-span->row_offset)return 0;
            for(uint32_t j=0;j<span->bottom-span->top;j++)
                if(s->rows[span->row_offset+j]>=image->height)return 0;
            expected_rows+=span->bottom-span->top;
            cursor=i+1u;
        }
    }
    return cursor==s->span_count && expected_rows==s->row_count;
}
int avp_vk_wall_layers_lookup(const AvpVkWallLayers *s,uint32_t x,
                               uint32_t y,uint32_t *slot,uint32_t *source_x,
                               uint32_t *source_y,int32_t *shade)
{
    if(!s||!s->column_start||x>=s->width||y>=s->height||
       !slot||!source_x||!source_y||!shade)return 0;
    *slot=AVP_VK_WALL_LAYER_NO_IMAGE;
    *source_x=*source_y=0u;*shade=0;
    for(uint32_t i=s->column_start[x];i<s->column_start[x+1u];i++){
        const AvpVkWallLayerSpan *span=&s->spans[i];
        if(y<span->top||y>=span->bottom)continue;
        *slot=span->source_slot;
        *source_x=span->source_x;
        *source_y=s->rows[span->row_offset+(y-span->top)];
        *shade=span->cry_shade;
    }
    return *slot!=AVP_VK_WALL_LAYER_NO_IMAGE;
}
