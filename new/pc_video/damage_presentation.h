#ifndef AVP_NEW_DAMAGE_PRESENTATION_H
#define AVP_NEW_DAMAGE_PRESENTATION_H

#include "jaguar/cry.h"

/* PC-only tick history. Never write the original pain, fade or vision state.
 * The same snapshot cadence as the camera and weapon pose is used. */
typedef struct AvpDamagePresentationSample {
    u16 color, extra;
    s16 species, level;
    unsigned valid;
} AvpDamagePresentationSample;

typedef struct AvpDamagePresentationBlend {
    u16 previous, current;
    unsigned weight; /* current endpoint, 0..256 */
} AvpDamagePresentationBlend;

static inline AvpDamagePresentationSample avp_damage_sample(
    u16 color,u16 extra,s16 species,s16 level,int normal_gameplay)
{
    AvpDamagePresentationSample s={color,extra,species,level,(unsigned)!!normal_gameplay};
    return s;
}

static inline AvpDamagePresentationBlend avp_damage_blend(
    AvpDamagePresentationSample previous,AvpDamagePresentationSample current,
    s16 fade_level,int smooth,double alpha)
{
    AvpDamagePresentationBlend b;
    b.current=avp_jaguar_game_clut(current.color,current.extra,fade_level);
    b.previous=b.current;b.weight=256u;
    /* Do not smooth death, transition fades, countdowns, vision changes,
     * or a previous actor/level. A missing snapshot is the retail path. */
    if(!smooth || !previous.valid || !current.valid || fade_level!=0 ||
       previous.species!=current.species || previous.level!=current.level ||
       previous.extra!=current.extra || previous.color==current.color ||
       !(alpha>=0.0 && alpha<1.0))return b;
    b.previous=avp_jaguar_game_clut(previous.color,current.extra,0);
    b.weight=(unsigned)(alpha*256.0+0.5);
    return b;
}

/* Interpolate the two actual Jaguar RMW results, not the packed CRY word
 * (its nibbles and intensity are signed and wrap). This also avoids the
 * visible quantization of interpolating a four-bit chroma component. */
static inline u32 avp_damage_blend_pixel(u32 pixel,AvpDamagePresentationBlend b,
    const u8 *q,size_t n)
{
    if(b.weight==256u || b.previous==b.current)
        return b.current?avp_jaguar_rmw_xrgb8888(pixel,b.current,q,n):pixel;
    u32 a=b.previous?avp_jaguar_rmw_xrgb8888(pixel,b.previous,q,n):pixel;
    if(!b.weight)return a;
    u32 z=b.current?avp_jaguar_rmw_xrgb8888(pixel,b.current,q,n):pixel;
    unsigned w=b.weight,iw=256u-w;
    unsigned r=(((a>>16)&255u)*iw+((z>>16)&255u)*w+128u)>>8;
    unsigned g=(((a>>8)&255u)*iw+((z>>8)&255u)*w+128u)>>8;
    unsigned bl=((a&255u)*iw+(z&255u)*w+128u)>>8;
    return (r<<16)|(g<<8)|bl;
}
#endif
