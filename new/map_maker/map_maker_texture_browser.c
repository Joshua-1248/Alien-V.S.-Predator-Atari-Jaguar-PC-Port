#include "map_maker_texture_browser.h"
#include <stddef.h>
#include <string.h>

int avp_mm_browser_layout(unsigned width,unsigned height,AvpMmBrowserLayout *l)
{
    unsigned pitch;
    if(!l||width<360u||height<310u)return 0;
    pitch=width>=1550u&&height>=680u?65u:
          (width>=1000u&&height>=510u?51u:38u);
    if(height<8u*pitch+85u)pitch=(height-85u)/8u;
    if(width<8u*pitch+28u)pitch=(width-28u)/8u;
    if(pitch<24u)return 0;
    l->pitch=pitch;
    l->swatch=pitch>=38u?pitch-10u:pitch-6u;
    l->x0=(int)width-(int)(pitch*8u)-18;
    l->y0=52;
    return 1;
}
int avp_mm_browser_hit(unsigned width,unsigned height,int x,int y,
                        unsigned page,unsigned *id)
{
    AvpMmBrowserLayout l;unsigned col,row;
    if(!id||page>3u||!avp_mm_browser_layout(width,height,&l)||
       x<l.x0||y<l.y0)return 0;
    col=(unsigned)(x-l.x0)/l.pitch;
    row=(unsigned)(y-l.y0)/l.pitch;
    if(col>=8u||row>=8u)return 0;
    *id=page*64u+row*8u+col;
    return 1;
}
static void pixel(uint32_t *dst,unsigned w,unsigned h,int x,int y,uint32_t color)
{
    if(x>=0&&y>=0&&(unsigned)x<w&&(unsigned)y<h)
        dst[(size_t)(unsigned)y*w+(unsigned)x]=color;
}
void avp_mm_browser_draw(const AvpMapMaker *m,uint32_t *dst,
                          unsigned width,unsigned height,
                          AvpMmBrowserLookup lookup,void *context)
{
    AvpMmBrowserLayout l;unsigned page;
    if(!m||!dst||!width||!height||!avp_mm_browser_layout(width,height,&l))return;
    page=m->palette_id/64u;
    for(unsigned row=0u;row<8u;row++)for(unsigned col=0u;col<8u;col++){
        unsigned id=page*64u+row*8u+col;
        unsigned field=m->mode_3d&&m->target_valid?m->target_field:
                         m->edit_mode_2d==3u?avp_mm_texture_field(m):m->field;
        AvpMmOverheadTexture t={0};
        int x0=l.x0+(int)(col*l.pitch),y0=l.y0+(int)(row*l.pitch);
        int selected=id==m->palette_id;
        if(id&&lookup)(void)lookup(context,m->level,id,field,&t);
        for(unsigned y=0;y<l.pitch;y++)for(unsigned x=0;x<l.pitch;x++){
            uint32_t rgb=selected?0xffd54au:0x4e5c6bu;
            if(x>=2u&&y>=2u&&x<l.swatch+2u&&y<l.swatch+2u){
                if(t.rgb&&t.width&&t.height){
                    unsigned tx=(unsigned)(((uint64_t)(x-2u)*t.width)/l.swatch);
                    unsigned ty=(unsigned)(((uint64_t)(y-2u)*t.height)/l.swatch);
                    if(t.width==t.height){
                        unsigned old;
                        switch(t.rotation&3u){
                        case 1:old=tx;tx=ty;ty=t.height-1u-old;break;
                        case 2:tx=t.width-1u-tx;ty=t.height-1u-ty;break;
                        case 3:old=tx;tx=t.width-1u-ty;ty=old;break;
                        default:break;
                        }
                    }
                    const uint8_t *p=t.rgb+((size_t)ty*t.width+tx)*3u;
                    rgb=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
                }else if(!id){rgb=((x/6u+y/6u)&1u)?0x39414du:0x14181fu;}
                else rgb=(x/5u+y/5u)&1u?0x2c3340u:0x1a1d27u;
            }else if(y>l.swatch+4u)rgb=0x070b12u;
            pixel(dst,width,height,x0+(int)x,y0+(int)y,rgb);
        }
    }
}
