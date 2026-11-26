#include "local_mp_life.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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

/* Source MAZE.S fades the current view on the Jaguar display clock. Co-op
 * must retain the outgoing view for each passenger; otherwise repeated host
 * rasterizations can expose a torn/animated source scene mid fade-out. */
void avp_mp_life_frame_release(AvpMpLifeFrame *frame)
{
    if(!frame)return;
    free(frame->pixels);frame->pixels=NULL;
    frame->width=frame->height=0u;
}
int avp_mp_life_frame_preserve(AvpMpLifeFrame *frame,const AvpMpLife *fade,
    uint32_t *pixels,unsigned width,unsigned height,unsigned pitch)
{
    size_t count;
    if(!frame||!fade||!pixels||!width||!height||pitch<width)return 0;
    if(fade->phase!=AVP_MP_LIFE_FADE_OUT &&
       fade->phase!=AVP_MP_LIFE_WAIT_LEVEL){
        avp_mp_life_frame_release(frame);return 1;
    }
    if(frame->pixels && (frame->width!=width||frame->height!=height))
        avp_mp_life_frame_release(frame);
    if(!frame->pixels){
        if((size_t)width>SIZE_MAX/(size_t)height/sizeof(uint32_t))return 0;
        count=(size_t)width*height;
        frame->pixels=(uint32_t *)malloc(count*sizeof(uint32_t));
        if(!frame->pixels)return 0;
        frame->width=width;frame->height=height;
        for(unsigned y=0;y<height;y++)
            memcpy(frame->pixels+(size_t)y*width,pixels+(size_t)y*pitch,
                   (size_t)width*sizeof(uint32_t));
    }
    for(unsigned y=0;y<height;y++)
        memcpy(pixels+(size_t)y*pitch,frame->pixels+(size_t)y*width,
               (size_t)width*sizeof(uint32_t));
    return 1;
}
int avp_mp_life_level_changed(unsigned *previous_level,unsigned level)
{
    if(!previous_level||!level)return 0;
    if(*previous_level==level)return 0;
    *previous_level=level;
    return 1;
}
