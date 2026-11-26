#define _POSIX_C_SOURCE 200809L
#define AVP_MICRO_TEST_SPARSE_HUD 1
#include "hud_replay_r428_reference.h"
#include "../hud_replay_fast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
static u32 a[320*228],b[320*228],rgb[65536];static u8 q[131072];static AvpHudRmwOps ops[320*228];
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static u32 rnd=123;static u32 next(void){rnd=rnd*1664525u+1013904223u;return rnd^(rnd>>16);}
int main(void){unsigned w=1920,h=1440;size_t n=(size_t)w*h;u32 *p=malloc(n*4),*r=malloc(n*4),*m=malloc(n*4),*s=malloc(n*4);assert(p&&r&&m&&s);for(unsigned i=0;i<65536;i++)rgb[i]=next()&0xffffff;for(unsigned i=0;i<sizeof q;i++)q[i]=next();
for(unsigned mode=0;mode<5;mode++){memset(ops,0,sizeof ops);for(unsigned i=0;i<320*228;i++){a[i]=next()&0xffffff;b[i]=a[i];unsigned x=i%320,y=i/320;int ink=mode==0?0:mode==1?(y>180 && x>40 && x<280):mode==2?(next()%100<10):mode==3?(next()%100<50):1;if(ink){if(next()&1)b[i]=next();else{ops[i].count=1;ops[i].ink[0]=(u16)next();b[i]=avp_micro_cry_rmw(a[i],ops[i].ink[0],q,sizeof q,rgb);}}}
for(size_t i=0;i<n;i++)p[i]=r[i]=next()&0xffffff;memset(m,0,n*4);memset(s,0,n*4);
AvpMicroHudReplay v={.host=p,.mask=m,.before=a,.after=b,.rgb=rgb,.ops=ops,.q=q,.qsize=sizeof q,.width=w,.stride=w};AvpBaseHudReplay z={.host=r,.mask=s,.before=a,.after=b,.rgb=rgb,.ops=ops,.q=q,.qsize=sizeof q,.width=w,.stride=w};avp_micro_hud_replay_init(&v,h);avp_base_hud_replay_init(&z,h);avp_micro_hud_replay_rows(&v,0,228);avp_base_hud_replay_rows(&z,0,228);assert(!memcmp(p,r,n*4)&&!memcmp(m,s,n*4));v.mask=NULL;z.mask=NULL;unsigned loops=mode<3?100:20;double base=0,fast=0;for(unsigned pass=0;pass<4;pass++){double t=now();if(pass==0||pass==3){for(unsigned j=0;j<loops;j++)avp_base_hud_replay_rows(&z,0,228);base+=now()-t;}else{for(unsigned j=0;j<loops;j++)avp_micro_hud_replay_rows(&v,0,228);fast+=now()-t;}}printf("mode %u baseline %.3f ms candidate %.3f ms speedup %.3fx\n",mode,base*500/loops,fast*500/loops,base/fast);}
free(p);free(r);free(m);free(s);return 0;}
