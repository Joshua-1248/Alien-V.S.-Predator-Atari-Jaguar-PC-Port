#ifndef AVP_MICRO_DAMAGE_CLUT_FAST_H
#define AVP_MICRO_DAMAGE_CLUT_FAST_H
#include "cry_rmw_fast.h"
#include "../pc_video/damage_presentation.h"
/* Same QCONV destination word for both original damage endpoints. Compute
 * it once and apply each signed Jaguar ink independently; interpolate only
 * the converted RGB results. No persistent color cache or changed rounding. */
static inline u16 avp_micro_damage_base(u32 dst,const u8 *q){
    const unsigned r=(dst>>16)&255u,g=(dst>>8)&255u,b=dst&255u;
    const unsigned y=r>g?(r>b?r:b):(g>b?g:b);
    unsigned c=0;
    if(y){const unsigned scale=avp_micro_qconv_recip[y];
        if(r==y)c=q[(((g*scale)>>8)<<8)|((b*scale)>>8)];
        else if(g==y)c=q[0x10000u|(((b*scale)>>8)<<8)|((r*scale)>>8)];
        else c=q[(((g*scale)>>8)<<8)|((r*scale)>>8)]^0xf0u;
    }
    return (u16)((c<<8)|y);
}
static inline u32 avp_micro_damage_endpoint(u16 base,u16 ink,const u32 rgb[65536]){
    const int c1=(int)(((ink>>8)&15u)^8u)-8,c2=(int)((ink>>12)^8u)-8;
    const int dy=(int)((ink&255u)^128u)-128;
    return rgb[(avp_micro_sat((int)(base>>12)+c2,15)<<12)|
        (avp_micro_sat((int)((base>>8)&15u)+c1,15)<<8)|
        avp_micro_sat((int)(base&255u)+dy,255)];
}
static inline u32 avp_micro_damage_blend(u32 px,AvpDamagePresentationBlend b,const u8 *q,size_t n,const u32 rgb[65536]){
    if(!q||n<131072u)return avp_damage_blend_pixel(px,b,q,n);
    if(b.weight==256u||b.previous==b.current)return avp_micro_cry_rmw(px,b.current,q,n,rgb);
    if(!b.weight)return avp_micro_cry_rmw(px,b.previous,q,n,rgb);
    const u16 base=avp_micro_damage_base(px,q);
    const u32 a=b.previous?avp_micro_damage_endpoint(base,b.previous,rgb):px;
    const u32 z=b.current?avp_micro_damage_endpoint(base,b.current,rgb):px;
    const unsigned w=b.weight,iw=256u-w;
    const unsigned r=(((a>>16)&255u)*iw+((z>>16)&255u)*w+128u)>>8;
    const unsigned g=(((a>>8)&255u)*iw+((z>>8)&255u)*w+128u)>>8;
    const unsigned bl=((a&255u)*iw+(z&255u)*w+128u)>>8;
    return (r<<16)|(g<<8)|bl;
}
#endif
