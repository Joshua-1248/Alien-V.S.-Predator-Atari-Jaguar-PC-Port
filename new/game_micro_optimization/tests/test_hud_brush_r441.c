#include "../hud_brush_fast.h"
#include "jaguar/avp_gpu/hud_weapon_icons.h"
#include "jaguar/avp_gpu/hud_fade.h"
#include <assert.h>
#include <stdio.h>
static unsigned seed=513;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed>>8;}
int main(void){
 u8 mask[64000]; AvpAmmoInfo ammo[6]={0}; unsigned long long comparisons=0;
 for(unsigned t=0;t<1200;t++){
  unsigned sp=t%3;int tracker=(t/3)%2,scope=(t/6)%2;
  int nx=(int)(rnd()%400)-40,ny=(int)(rnd()%280)-40,nw=(int)(rnd()%140)-10,nh=(int)(rnd()%90)-10,weapons=(int)(rnd()%9)-1;
  for(int i=0;i<6;i++){ammo[i].x=(s16)((int)(rnd()%400)-40);ammo[i].y=(s16)((int)(rnd()%280)-40);}
  avp_micro_hud_exclusions(mask,sp,tracker,scope,nx,ny,nw,nh,ammo,weapons);
  for(int y=14;y<214;y++)for(int x=0;x<320;x++){
   int value=(sp==0u&&((x>=151&&x<231&&y>=22&&y<31)||(x>=43&&x<59&&y>=104&&y<113)))||(sp==1u&&x>=148&&x<228&&y>=24&&y<34)||(sp==2u&&x>=111&&x<221&&y>=24&&y<36);
   int energy=x>=nx&&x<nx+nw&&y>=ny+14&&y<ny+14+nh;
   int meter=sp==2u&&scope&&x>=AVP_PRED_SCOPE_X&&x<AVP_PRED_SCOPE_X+AVP_PRED_SCOPE_W&&y>=AVP_PRED_SCOPE_Y+14&&y<AVP_PRED_SCOPE_Y+14+AVP_PRED_SCOPE_H;
   int am=0;for(int i=0;i<weapons&&i<6;i++)if(x>=ammo[i].x&&x<ammo[i].x+44&&y>=ammo[i].y+14&&y<ammo[i].y+22){am=1;break;}
   int tr=sp==0u&&tracker&&((x>=16&&x<112&&y>=125&&y<214)||(x>=112&&x<128&&y>=212&&y<214));
   int blocked=value||energy||meter||am||tr;
   assert(mask[(y-14)*320+x]==blocked);
   u8 pi=(u8)(rnd()%33); unsigned owned=rnd()&127; int in=rnd()%7,out=rnd()%7,ib=(int)(rnd()%12)-2,ob=(int)(rnd()%12)-2;
   u8 a=avp_gpu_weapon_icon_pixel(pi,sp,x,y-14,owned,in,ib,out,ob);
   if(sp==2)a=avp_gpu_predator_faded_icon(a,x,y-14,ib,ob);
   if(blocked)a=0;
   u8 b=0;if(pi&&!mask[(y-14)*320+x]){b=avp_gpu_weapon_icon_pixel(pi,sp,x,y-14,owned,in,ib,out,ob);if(sp==2)b=avp_gpu_predator_faded_icon(b,x,y-14,ib,ob);}
   assert(a==b);comparisons++;
  }
 }
 printf("PASS %llu exact brush index and exclusion comparisons\n",comparisons);return 0;
}
