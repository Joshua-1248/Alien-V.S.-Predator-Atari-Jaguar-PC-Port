#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
u8 fullbits[256];
static u8 map[64*64*8];
static u32 rng=0x9e3779b9u;
static u32 next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static unsigned long count;
static void check(u8 h,s32 x,s32 y,u16 w){
 avp_game_micro_opt_set(0);u8 a=SafePos(h,x,y,w);
 avp_game_micro_opt_set(1);u8 b=SafePos(h,x,y,w);
 if(a!=b){fprintf(stderr,"Mismatch %u %08x %08x %u: %u %u\n",h,(u32)x,(u32)y,w,a,b);exit(1);}++count;
}
static void populate(unsigned layout){
 memset(fullbits,0,sizeof(fullbits));fullbits[1]=AVP_WB_SOLID;fullbits[32]=AVP_WB_DOOR;fullbits[33]=AVP_WB_DOOR|AVP_WB_TBDOOR;fullbits[160]=AVP_WB_DOOR|AVP_WB_EDGEDOOR;fullbits[40]=AVP_WB_LEFTONLY;
 for(unsigned i=0;i<sizeof(map);i+=8){for(unsigned j=0;j<4;j++)map[i+j]=layout==0?0:layout==1?(u8[]){0,0,0,1}[next()%4]:layout==2?(u8[]){32,33,160,0}[next()%4]:(u8[]){0,1,32,33,160,40,0,0}[next()%8];map[i+6]=(u8)next();map[i+7]=(u8)next();}
}
static s32 xx[8192],yy[8192];static volatile unsigned sink;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void bench(void){avp_collision_bind_maze(map,64,64);rng=0x9e3779b9u;
 for(unsigned l=0;l<4;l++){populate(l);for(unsigned i=0;i<8192;i++){unsigned x=1+next()%62,y=1+next()%62;xx[i]=(s32)((x<<16)|(next()&1?0x1000:0xf000));yy[i]=(s32)((y<<16)|(next()&1?0x1000:0xf000));}
 for(unsigned r=0;r<7;r++){avp_game_micro_opt_set(1);unsigned sum=0;double t=now();for(unsigned i=0;i<4000000;i++)sum+=SafePos(80,xx[i&8191],yy[i&8191],40);sink=sum;printf("layout %u %.3f ns checksum %u\n",l,(now()-t)*1e9/4000000,sink);}}
}
int main(int argc,char **argv){(void)argv;if(argc==1){
 const unsigned dims[][2]={{1,1},{1,64},{64,1},{7,19},{64,64},{0,0}};
 for(unsigned k=0;k<6;k++){unsigned w=dims[k][0],h=dims[k][1];avp_collision_bind_maze(map,(u16)w,(u16)h);
 for(unsigned i=0;i<256;i++)fullbits[i]=(u8)next();
 for(unsigned i=0;i<sizeof(map);i++)map[i]=(u8)next();
 for(unsigned i=0;i<400000;i++){unsigned x=next()%(w+2),y=next()%(h+2);u16 width=(u16)(1+next()%63);unsigned near=width<<9;unsigned fx=next()%near,fy=next()%near;if(next()&1)fx=65535-fx;if(next()&1)fy=65535-fy;check((u8)next(),(s32)((x<<16)|fx),(s32)((y<<16)|fy),width);map[next()%sizeof(map)]=(u8)next();fullbits[next()%256]=(u8)next();}}
 avp_collision_bind_maze(map,64,64);for(unsigned l=0;l<4;l++){populate(l);for(unsigned i=0;i<200000;i++){unsigned x=next()%64,y=next()%64;unsigned fx=next()%20480,fy=next()%20480;if(next()&1)fx=65535-fx;if(next()&1)fy=65535-fy;check((u8)next(),(s32)((x<<16)|fx),(s32)((y<<16)|fy),40);}}
 avp_collision_bind_maze(NULL,64,64);check(80,0x11000,0x11000,40);printf("SafePos corner parity: %lu cases PASS\n",count);return 0;}bench();return 0;}
