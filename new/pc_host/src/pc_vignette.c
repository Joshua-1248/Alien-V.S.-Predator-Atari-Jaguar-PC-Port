#include "pc/pc_vignette.h"
#include <stdlib.h>

/* Radial falloff in the final viewport's normalized coordinates (not the
 * Jaguar projection).  x^2+y^2 = 0 at center, nearly 2 at corners.
 * The smoothstep eliminates a harsh visible inner ring. All per-frame work
 * uses the precomputed 8-bit weights; no per-frame sqrt/pow/floats. */
int avp_pc_vignette_prepare(AvpPcVignetteMask *cache,unsigned width,unsigned height)
{
    uint8_t *weights;
    size_t n;
    if(!cache||!width||!height||width>SIZE_MAX/height)return 0;
    if(cache->weights&&cache->width==width&&cache->height==height)return 1;
    n=(size_t)width*height;
    weights=(uint8_t *)malloc(n);
    if(!weights)return 0;
    for(unsigned y=0;y<height;y++){
        const uint32_t dy=(uint32_t)(((uint64_t)(2u*(uint64_t)y+1u>height?
            2u*(uint64_t)y+1u-height:height-(2u*(uint64_t)y+1u))*1024u)/height);
        for(unsigned x=0;x<width;x++){
            const uint32_t dx=(uint32_t)(((uint64_t)(2u*(uint64_t)x+1u>width?
                2u*(uint64_t)x+1u-width:width-(2u*(uint64_t)x+1u))*1024u)/width);
            const uint32_t r2=dx*dx+dy*dy;
            unsigned t=0u;
            if(r2>=2100000u)t=255u;
            else if(r2>400000u)t=(unsigned)(((uint64_t)(r2-400000u)*255u)/1700000u);
            /* smoothstep: t*t*(3-2*t) with t in 0..255. */
            weights[(size_t)y*width+x]=(uint8_t)
                ((t*t*(765u-2u*t)+32512u)/65025u);
        }
    }
    /* The full mask remains the single source of truth. Edge spans are
     * optional: on allocation failure the old full-row loop still works. */
    unsigned *left_count=(unsigned*)malloc((size_t)height*sizeof(unsigned));
    unsigned *right_count=(unsigned*)malloc((size_t)height*sizeof(unsigned));
    if(!left_count||!right_count){
        free(left_count);free(right_count);
        left_count=right_count=NULL;
    }
    free(cache->weights);
    free(cache->left_count);
    free(cache->right_count);
    cache->left_count=left_count;
    cache->right_count=right_count;
    cache->span_valid=0u;
    cache->weights=weights;
    cache->width=width;
    cache->height=height;
    return 1;
}

void avp_pc_vignette_destroy(AvpPcVignetteMask *cache)
{
    if(!cache)return;
    free(cache->weights);
    free(cache->left_count);
    free(cache->right_count);
    cache->left_count=cache->right_count=NULL;
    cache->span_valid=0u;
    cache->weights=NULL;
    cache->width=cache->height=0u;
}

void avp_pc_vignette_apply(AvpPcVignetteMask *cache,
    uint32_t *pixels,unsigned pitch,unsigned frame_width,unsigned frame_height,
    unsigned x,unsigned y,unsigned width,unsigned height,unsigned strength)
{
    if(!cache||!pixels||!strength||!width||!height||strength>100u||
       frame_width>pitch||x>frame_width||y>frame_height||
       width>frame_width-x||height>frame_height-y)return;
    if(!avp_pc_vignette_prepare(cache,width,height))return;
    /* R390: the vignette has a contiguous zero-opacity middle interval on
     * each row. Calculate its exact bounds only when dimensions or intensity
     * change. At constant intensity, never read/touch pixels in that region. */
    if(cache->left_count && cache->right_count &&
       (!cache->span_valid||cache->span_strength!=strength)){
        for(unsigned row=0;row<height;row++){
            const uint8_t *mask=cache->weights+(size_t)row*width;
            unsigned left=0u,right=0u;
            while(left<width && (mask[left]*strength+50u)/100u)left++;
            if(left<width){
                while(right<width-left &&
                      (mask[width-1u-right]*strength+50u)/100u)right++;
            }
            cache->left_count[row]=left;
            cache->right_count[row]=right;
        }
        cache->span_strength=strength;
        cache->span_valid=1u;
    }
    for(unsigned row=0;row<height;row++){
        uint32_t *out=pixels+(size_t)(y+row)*pitch+x;
        const uint8_t *mask=cache->weights+(size_t)row*width;
        const unsigned left=cache->span_valid?cache->left_count[row]:width;
        const unsigned right=cache->span_valid?cache->right_count[row]:0u;
        /* Original integer rounding and channel packing are unchanged. */
        for(unsigned span=0;span<2u;span++){
            const unsigned begin=span?width-right:0u;
            const unsigned end=span?width:left;
            for(unsigned col=begin;col<end;col++){
                const unsigned opacity=(mask[col]*strength+50u)/100u;
                if(opacity){
                    const uint32_t color=out[col];
                    const unsigned brightness=256u-opacity;
                    out[col]=((color&0x00ff00ffu)*brightness>>8&0x00ff00ffu)|
                             ((color&0x0000ff00u)*brightness>>8&0x0000ff00u);
                }
            }
        }
    }
}
