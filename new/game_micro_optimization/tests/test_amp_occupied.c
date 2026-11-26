#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "amp.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
u8 fullbits[256];
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
static AvpAmp amps[AVP_NUM_AMPS];
AvpAmp *amp_data=amps;
static uint32_t state=0x38aa3871u;
static uint32_t rng(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void active(AvpAmp *a){(void)a;}
static unsigned count;
static void check(s32 x,s32 y){avp_game_micro_opt_set(0);int a=AMPCollisions(x,y);avp_game_micro_opt_set(1);int b=AMPCollisions(x,y);if(a!=b){fprintf(stderr,"mismatch %x %x %d %d\n",(unsigned)x,(unsigned)y,a,b);exit(1);}count++;}
static s32 coord(unsigned cell,unsigned frac){return (s32)((cell<<16)|frac);}
static void set_actor(unsigned n,unsigned x,unsigned y,unsigned fx,unsigned fy,unsigned type,unsigned mode){amps[n].xpos=coord(x,fx);amps[n].ypos=coord(y,fy);amps[n].creature=(u16)type;amps[n].mode=mode==1?active:NULL;amps[n].host_static=mode==2;collmap[y*64+x]=(u16)(n+1);}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static s32 xs[4096],ys[4096];
static void bench(unsigned layout,int enabled){avp_game_micro_opt_set(enabled);unsigned sum=0;double t=now();for(unsigned i=0;i<5000000;i++)sum+=(unsigned)AMPCollisions(xs[i&4095],ys[i&4095]);printf("layout %u %s %.3f ns checksum %u\n",layout,enabled?"ON":"OFF",(now()-t)*1e9/5000000,sum);}
int main(int argc,char **argv){(void)argv;
 if(argc==1){
  static const unsigned fr[]={0,1,4351,4352,4353,15231,15232,15233,17407,17408,17409,21759,21760,21761,25600,30720,32768,34815,34816,34817,65534,65535};
  for(unsigned type=0;type<42;type++)for(unsigned mode=0;mode<3;mode++)for(unsigned i=0;i<sizeof(fr)/sizeof(*fr);i++)for(unsigned j=0;j<sizeof(fr)/sizeof(*fr);j++){
   memset(collmap,0,sizeof(collmap));set_actor(0,20,20,fr[i],fr[j],type,mode);check(coord(20,0),coord(20,0));check(coord(20,65535),coord(20,65535));check(coord(21,0),coord(20,fr[j]));check(coord(19,65535),coord(20,fr[j]));check(coord(20,fr[i]),coord(21,0));check(coord(20,fr[i]),coord(19,65535));
  }
  for(unsigned i=0;i<600000;i++){
   unsigned x=1+rng()%62,y=1+rng()%62;
   for(unsigned q=0;q<9;q++){unsigned ax=x+q%3-1,ay=y+q/3-1;unsigned mode=rng()%3;set_actor(q,ax,ay,rng()&65535,rng()&65535,rng()%43,mode);switch(rng()%5){case 0:collmap[ay*64+ax]=0;break;case 1:collmap[ay*64+ax]=65535;break;case 2:amps[q].xpos=coord(rng()%64,rng()&65535);break;default:break;}}
   check(coord(x,rng()&65535),coord(y,rng()&65535));
   for(unsigned q=0;q<9;q++)collmap[(y+q/3-1)*64+x+q%3-1]=0;
  }
  printf("occupied actor parity %u cases PASS\n",count);
 }
 for(unsigned layout=0;layout<4;layout++){
  memset(collmap,0,sizeof(collmap));
  for(unsigned n=0;n<AVP_NUM_AMPS;n++){unsigned x=2+n%20,y=2+n/20; if(layout==1)collmap[y*64+x]=65535;else if(layout>=2)set_actor(n,x,y,32768,32768,n%40,1);}
  for(unsigned i=0;i<4096;i++){unsigned x=2+rng()%20,y=2+rng()%15;if(layout==2 && i%2)x+=24;xs[i]=coord(x,rng()&65535);ys[i]=coord(y,rng()&65535);}
  for(unsigned r=0;r<3;r++){bench(layout,r&1);bench(layout,!(r&1));}
 }
 return 0;
}
