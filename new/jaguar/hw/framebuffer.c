#include "jaguar/framebuffer.h"
#include <stdlib.h>

int avp_jaguar_framebuffer_init(AvpJaguarFramebuffer *fb,unsigned width,unsigned height)
{
    size_t n;
    u32 *p;
    if(!fb||!width||!height)return 0;
    if(fb->pixels&&fb->width==width&&fb->height==height)return 1;
    if((size_t)width>SIZE_MAX/(size_t)height)return 0;
    n=(size_t)width*(size_t)height;
    if(n>SIZE_MAX/sizeof(*p))return 0;
    p=(u32*)calloc(n,sizeof(*p));
    if(!p)return 0;
    free(fb->pixels);
    fb->pixels=p;fb->width=width;fb->height=height;
    return 1;
}

void avp_jaguar_framebuffer_destroy(AvpJaguarFramebuffer *fb)
{
    if(!fb)return;
    free(fb->pixels);
    fb->pixels=NULL;fb->width=0;fb->height=0;
}

void avp_jaguar_framebuffer_clear(AvpJaguarFramebuffer *fb,u32 color)
{
    size_t i,n;
    if(!fb||!fb->pixels)return;
    n=(size_t)fb->width*(size_t)fb->height;
    for(i=0;i<n;i++)fb->pixels[i]=color;
}

size_t avp_jaguar_framebuffer_count(const AvpJaguarFramebuffer *fb)
{
    return fb?(size_t)fb->width*(size_t)fb->height:0u;
}
