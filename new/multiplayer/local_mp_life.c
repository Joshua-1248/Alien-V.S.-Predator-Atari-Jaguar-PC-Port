#include "local_mp_life.h"
int avp_mp_life_busy(const AvpMpLife *l)
{return l&&l->phase!=AVP_MP_LIFE_ALIVE;}
void avp_mp_life_begin(AvpMpLife *l)
{if(l&&l->phase==AVP_MP_LIFE_ALIVE){l->phase=AVP_MP_LIFE_FADE_OUT;l->fade=0;}}
int avp_mp_life_display_step(AvpMpLife *l)
{
    if(!l)return 0;
    if(l->phase==AVP_MP_LIFE_FADE_OUT){
        l->fade=l->fade+2u>=128u?128u:l->fade+2u;
        if(l->fade==128u){l->phase=AVP_MP_LIFE_WAIT_LEVEL;return 1;}
    }else if(l->phase==AVP_MP_LIFE_FADE_IN){
        l->fade=l->fade<2u?0u:l->fade-2u;
        if(!l->fade)l->phase=AVP_MP_LIFE_ALIVE;
    }
    return 0;
}
void avp_mp_life_resumed(AvpMpLife *l)
{if(l&&l->phase==AVP_MP_LIFE_WAIT_LEVEL){l->phase=AVP_MP_LIFE_FADE_IN;l->fade=128u;}}
void avp_mp_life_wait(AvpMpLife *l)
{if(l){l->phase=AVP_MP_LIFE_WAIT_LEVEL;l->fade=128u;}}
void avp_mp_life_apply(const AvpMpLife *l,uint32_t *pixels,
                       unsigned width,unsigned height,unsigned pitch)
{
    if(!l||!pixels||!l->fade||l->fade>128u||pitch<width)return;
    const unsigned light=128u-l->fade;
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
        uint32_t c=pixels[(size_t)y*pitch+x];
        unsigned r=((c>>16)&255u)*light/128u;
        unsigned g=((c>>8)&255u)*light/128u;
        unsigned b=(c&255u)*light/128u;
        pixels[(size_t)y*pitch+x]=(c&0xff000000u)|(r<<16)|(g<<8)|b;
    }
}
