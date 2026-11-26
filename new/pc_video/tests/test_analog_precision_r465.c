/* Exhaustive raw-axis sweep plus fractional heading and movement endpoints. */
#include "pc/pc_axis_deadzone.h"
#include "pc/pc_analog_aim.h"
#include "pc/pc_live_heading.h"
#include "../../pc_video/pc_analog_velocity.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
int main(void){
 unsigned long long cases=0;
 for(int d=0;d<=95;d+=5)for(int m=0;m<=1;m++)for(int inv=0;inv<=1;inv++){
   float dz=d/100.0f,prev=inv?1.0f:-1.0f;
   for(int i=-32768;i<=32767;i++){
     float v=avp_pc_axis_precision_sample((int16_t)i,dz,inv,m);
     assert(isfinite(v)&&v>=-1.0f&&v<=1.0f);
     assert(inv?v<=prev:v>=prev);
     if(fabsf(avp_pc_axis_deadzone((int16_t)i,dz,inv))<=dz)assert(v==0.0f);
     prev=v;cases++;
   }
   assert(avp_pc_axis_precision_sample(-32768,dz,inv,m)==(inv?1.0f:-1.0f));
   assert(avp_pc_axis_precision_sample(32767,dz,inv,m)==(inv?-1.0f:1.0f));
 }
 for(int m=0;m<=1;m++){
   float v=avp_pc_axis_precision(0.21f,0.2f,m);
   assert(v>0&&v<0.005f);
   assert(avp_pc_axis_precision(-0.21f,0.2f,m)==-v);
   printf("21%% raw: %s %.7f output vs old 0.21\n",m?"movement":"turn",v);
 }
 for(int species=0;species<3;species++)for(int dir=0;dir<4;dir++){
   unsigned cap=species==1?17000:11900;
   int32_t vx=0,vy=0;
   double f=dir<2?(dir==0?1.0:-1.0):0.0,s=dir>=2?(dir==2?1.0:-1.0):0.0;
   float fine=avp_pc_axis_precision(.21f,.2f,1);
   for(int t=0;t<100;t++)avp_pc_analog_velocity(&vx,&vy,32767,0,f*fine,s*fine,0,0,0,0,cap,2);
   double mag=avp_pc_analog_metric(vx,vy)/3;
   assert(mag>0&&mag<cap/200.0);
   for(int t=0;t<100;t++)avp_pc_analog_velocity(&vx,&vy,32767,0,f,s,0,0,0,0,cap,2);
   assert(avp_pc_analog_metric(vx,vy)==cap*3.0);
 }
 for(int i=0;i<2;i++){
   double angle=i?90.75:31.25;
   uint32_t word=(uint32_t)llround(angle*(4294967296.0/360.0));
   AvpPcContinuousHeading h={0};
   for(int j=0;j<1000;j++)assert(avp_pc_continuous_heading_tick(&h,word,0,1,1,1000)==word);
   AvpPcLiveHeading live={0};
   assert(avp_pc_live_heading_poll(&live,word,1000000,0,1,1,1000,10)==word);
   assert(avp_pc_live_heading_poll(&live,word,3000000,0,1,1,1000,10)==word);
   float axis=avp_pc_axis_precision(.21f,.2f,0);
   assert(avp_pc_live_heading_poll(&live,word,5000000,axis,1,1,1000,10)==word);
   uint32_t stop=avp_pc_live_heading_poll(&live,word,7000000,0,1,1,1000,10);
   assert(stop!=word);
   assert(avp_pc_live_heading_poll(&live,word,9000000,0,1,1,1000,10)==stop);
 }
 printf("PASS %llu raw axis cases, 12 native velocity directions, 2 fractional heading release cases\n",cases);
 return 0;
}
