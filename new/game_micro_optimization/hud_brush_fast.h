#ifndef AVP_MICRO_HUD_BRUSH_FAST_H
#define AVP_MICRO_HUD_BRUSH_FAST_H
#include "collectables.h"
#include "jaguar/avp_gpu/pred_scope_raster.h"
#include <string.h>
/* Rebuild exclusions from live HUD values every draw. Coordinates are HUD-local. */
static inline void avp_micro_hud_exclude_rect(u8 *mask,int x,int y,int w,int h)
{
    int x1=x+w,y1=y+h;
    if(x<0)x=0;
    if(y<0)y=0;
    if(x1>320)x1=320;
    if(y1>200)y1=200;
    if(x>=x1||y>=y1)return;
    for(int row=y;row<y1;row++)memset(mask+(size_t)row*320u+x,1,(size_t)(x1-x));
}
static inline void avp_micro_hud_exclusions(u8 *mask,unsigned sp,int tracker,int scope,
    int nx,int ny,int nw,int nh,const AvpAmmoInfo *ammo,int weapons)
{
    memset(mask,0,320u*200u);
    if(sp==0u){avp_micro_hud_exclude_rect(mask,151,8,80,9);avp_micro_hud_exclude_rect(mask,43,90,16,9);}
    if(sp==1u)avp_micro_hud_exclude_rect(mask,148,10,80,10);
    if(sp==2u)avp_micro_hud_exclude_rect(mask,111,10,110,12);
    avp_micro_hud_exclude_rect(mask,nx,ny,nw,nh);
    if(sp==2u&&scope)avp_micro_hud_exclude_rect(mask,AVP_PRED_SCOPE_X,AVP_PRED_SCOPE_Y,AVP_PRED_SCOPE_W,AVP_PRED_SCOPE_H);
    for(int i=0;i<weapons&&i<6;i++)avp_micro_hud_exclude_rect(mask,ammo[i].x,ammo[i].y,44,8);
    if(sp==0u&&tracker){avp_micro_hud_exclude_rect(mask,16,111,96,89);avp_micro_hud_exclude_rect(mask,112,198,16,2);}
}
#endif
