#ifndef AVP_PC_SCANLINES_H
#define AVP_PC_SCANLINES_H
/* PC output-pixel scanlines. Independent of original gameplay, resolution and timing.
 * The GLSL counterpart uses the same unsigned integer equations. */
#include <stdint.h>
#include <stddef.h>
typedef struct AvpPcScanlines {
    uint32_t enabled, style, thickness, spacing, strength, phase;
} AvpPcScanlines;
enum { AVP_SCAN_HORIZONTAL, AVP_SCAN_HORIZONTAL_SOFT, AVP_SCAN_VERTICAL,
       AVP_SCAN_VERTICAL_SOFT, AVP_SCAN_GRID,
       AVP_SCAN_DIAGONAL_DOWN, AVP_SCAN_DIAGONAL_DOWN_SOFT,
       AVP_SCAN_DIAGONAL_UP, AVP_SCAN_DIAGONAL_UP_SOFT,
       AVP_SCAN_DIAGONAL_GRID, AVP_SCAN_DIAGONAL_GRID_SOFT,
       AVP_SCAN_GRID_SOFT, AVP_SCAN_STYLE_COUNT };
static inline AvpPcScanlines avp_scanline_defaults(void)
{
    AvpPcScanlines s={0,AVP_SCAN_HORIZONTAL,1,2,35,0};return s;
}
static inline int avp_scanline_active(const AvpPcScanlines *s)
{
    return s && s->enabled==1u && s->style<AVP_SCAN_STYLE_COUNT &&
        s->thickness>=1u && s->thickness<=16u && s->spacing>=1u &&
        s->spacing<=16u && s->strength>=1u && s->strength<=100u && s->phase<=31u;
}
static inline unsigned avp_scanline_axis(const AvpPcScanlines *s,unsigned pos,int soft)
{
    const unsigned period=s->thickness+s->spacing;
    const unsigned p=(pos%period+s->phase)%period;
    unsigned a=(s->strength*256u+50u)/100u;
    if(p>=s->thickness)return 256u;
    if(soft){
        const unsigned edge=p<s->thickness-1u-p?p:s->thickness-1u-p;
        const unsigned peak=2u*((s->thickness-1u)/2u)+1u;
        a=(a*(2u*edge+1u)+peak/2u)/peak;
    }
    return 256u-a;
}
/* Caller establishes valid active settings before invoking the kernel. */
static inline unsigned avp_scanline_weight(const AvpPcScanlines *s,unsigned x,unsigned y)
{
    if(s->style==AVP_SCAN_GRID||s->style==AVP_SCAN_GRID_SOFT){
        const int soft=s->style==AVP_SCAN_GRID_SOFT;
        return avp_scanline_axis(s,x,soft)*avp_scanline_axis(s,y,soft)/256u;
    }
    if(s->style>=AVP_SCAN_DIAGONAL_DOWN){
        const unsigned period=s->thickness+s->spacing;
        const unsigned down=(x%period+period-y%period)%period;
        const unsigned up=(x%period+y%period)%period;
        const int soft=s->style==AVP_SCAN_DIAGONAL_DOWN_SOFT||
            s->style==AVP_SCAN_DIAGONAL_UP_SOFT||s->style==AVP_SCAN_DIAGONAL_GRID_SOFT;
        if(s->style==AVP_SCAN_DIAGONAL_GRID||s->style==AVP_SCAN_DIAGONAL_GRID_SOFT)
            return avp_scanline_axis(s,down,soft)*avp_scanline_axis(s,up,soft)/256u;
        return avp_scanline_axis(s,s->style<=AVP_SCAN_DIAGONAL_DOWN_SOFT?down:up,soft);
    }
    if(s->style==AVP_SCAN_VERTICAL||s->style==AVP_SCAN_VERTICAL_SOFT)
        return avp_scanline_axis(s,x,s->style==AVP_SCAN_VERTICAL_SOFT);
    return avp_scanline_axis(s,y,s->style==AVP_SCAN_HORIZONTAL_SOFT);
}
static inline uint32_t avp_scanline_darken(uint32_t pixel,unsigned w)
{
    return (pixel&0xff000000u)|(((pixel&0x00ff00ffu)*w>>8)&0x00ff00ffu)|
        (((pixel&0x0000ff00u)*w>>8)&0x0000ff00u);
}
static inline void avp_scanline_apply(const AvpPcScanlines *s,uint32_t *pixels,
    unsigned pitch,unsigned width,unsigned height)
{
    unsigned weights[32],period;
    if(!avp_scanline_active(s)||!pixels||!width||!height||pitch<width||
       (size_t)height>SIZE_MAX/sizeof(*pixels)/pitch)return;
    period=s->thickness+s->spacing;
    if(s->style==AVP_SCAN_HORIZONTAL||s->style==AVP_SCAN_HORIZONTAL_SOFT){
        for(unsigned y=0;y<height;y++){
            unsigned w=avp_scanline_axis(s,y,s->style==AVP_SCAN_HORIZONTAL_SOFT);
            if(w==256u)continue;
            uint32_t *row=pixels+(size_t)y*pitch;
            for(unsigned x=0;x<width;x++)row[x]=avp_scanline_darken(row[x],w);
        }
    }else if(s->style>=AVP_SCAN_DIAGONAL_DOWN){
        /* All patterns repeat in both output axes. Cache a tile (<=4 KiB)
         * once per frame; no division or diagonal calculation per pixel. */
        unsigned tile[32][32];
        for(unsigned y=0;y<period;y++)for(unsigned x=0;x<period;x++)
            tile[y][x]=avp_scanline_weight(s,x,y);
        unsigned yp=0u;
        for(unsigned y=0;y<height;y++){
            uint32_t *row=pixels+(size_t)y*pitch;
            const unsigned *line=tile[yp];
            unsigned xp=0u;
            for(unsigned x=0;x<width;x++){
                unsigned w=line[xp];
                if(w!=256u)row[x]=avp_scanline_darken(row[x],w);
                if(++xp==period)xp=0u;
            }
            if(++yp==period)yp=0u;
        }
    }else{
        for(unsigned x=0;x<period;x++)weights[x]=avp_scanline_axis(s,x,s->style==AVP_SCAN_VERTICAL_SOFT);
        for(unsigned y=0;y<height;y++){
            unsigned yw=s->style==AVP_SCAN_GRID?avp_scanline_axis(s,y,0):256u;
            uint32_t *row=pixels+(size_t)y*pitch;
            unsigned phase=0u;
            for(unsigned x=0;x<width;x++){
                unsigned w=weights[phase]*yw/256u;
                if(w!=256u)row[x]=avp_scanline_darken(row[x],w);
                if(++phase==period)phase=0u;
            }
        }
    }
}
#endif
