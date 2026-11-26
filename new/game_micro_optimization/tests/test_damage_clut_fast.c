#include "../damage_clut_fast.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
static u32 rng=426;static u32 next(void){return rng=rng*1664525u+1013904223u;}
static u8 q[131072];static u32 rgb[65536],src[2560*1440],dst[2560*1440];static volatile u32 sink;
static double sec(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double run(int fast,AvpDamagePresentationBlend b,u32 *hash){double t=sec();u32 h=0;for(unsigned k=0;k<4;k++){
 for(unsigned i=0;i<2560u*1440u;i++)dst[i]=fast?avp_micro_damage_blend(src[i],b,q,sizeof q,rgb):avp_damage_blend_pixel(src[i],b,q,sizeof q);
 h=h*16777619u^dst[k*113u];}*hash=h;sink=h;return sec()-t;}
int main(int argc,char**argv){(void)argv;for(unsigned i=0;i<sizeof q;i++)q[i]=(u8)(next()>>24);const char *path=getenv("AVP_TEST_QCONV");if(path){FILE *f=fopen(path,"rb");assert(f);assert(fread(q,1,sizeof q,f)==sizeof q);assert(fgetc(f)==EOF);fclose(f);puts("Using privately decoded retail QCONV table");}for(unsigned i=0;i<65536;i++){u8 r,g,b;avp_jaguar_cry16_to_rgb((u16)i,&r,&g,&b);rgb[i]=((u32)r<<16)|((u32)g<<8)|b;}
 if(argc>1){for(unsigned i=0;i<2560u*1440u;i++)src[i]=rgb[(next()>>16)&65535u];AvpDamagePresentationBlend cases[]={{0x7121,0x5035,127},{0,0x5035,127},{0x7121,0,127}};for(unsigned j=0;j<3;j++){u32 h1,h2;double a=run(0,cases[j],&h1),b=run(1,cases[j],&h2);assert(h1==h2);b+=run(1,cases[j],&h2);a+=run(0,cases[j],&h1);assert(h1==h2);printf("case=%u base=%.6f candidate=%.6f ratio=%.3fx hash=%08x\n",j,a,b,a/b,h1);}return 0;}
 unsigned long long count=0;for(unsigned i=0;i<4000000;i++){u32 p=next();AvpDamagePresentationBlend b={(u16)(next()>>16),(u16)(next()>>16),next()%257};if(i%7==0)b.previous=0;if(i%11==0)b.current=0;if(i%13==0)b.previous=b.current;assert(avp_micro_damage_blend(p,b,q,sizeof q,rgb)==avp_damage_blend_pixel(p,b,q,sizeof q));count++;}
 for(unsigned c=0;c<65536;c++)for(unsigned w=0;w<=256;w+=16){u32 p=rgb[c]|0xa5000000u;AvpDamagePresentationBlend b={0x7121,0x5035,w};assert(avp_micro_damage_blend(p,b,q,sizeof q,rgb)==avp_damage_blend_pixel(p,b,q,sizeof q));count++;}
 printf("PASS %llu exact damage blend pixels\n",count);}
