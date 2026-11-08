/* Independent R249 regression oracle: preserve TestSpark outcomes across signed width words. */
#include "player.h"
#include "maze.h"
#include "mazescrn.h"
#include "amp.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
AvpAmp amps[AVP_NUM_AMPS],*amps_at=amps;
u32 x_pos=0x100000u,y_pos=0x100000u,centre_angle=0,fire_angle=0;
s16 swing_x=0;
s16 fire_damage=30;
u32 fire_distance=0x90u<<8;
u16 fire_width=20;
u32 FireDistance(void){return 0;}
s32 cos_d0(u16 a){(void)a;return 0x7fff;}
s32 sin_d0(u16 a){(void)a;return 0;}
static void acting(AvpAmp *a){(void)a;}
void make_spark_at(s32 x,s32 y,int blood){(void)x;(void)y;(void)blood;}
void make_spark_hit_at(s32 x,s32 y,const AvpAmp*a){(void)x;(void)y;(void)a;}
static uint64_t accum=14695981039346656037ull;
static void mix(uint32_t v){accum^=v;accum*=1099511628211ull;}
int main(void){
    const u16 widths[]={0,1,2,10,20,100,32767,32768,50000,65535};
    const s32 offsets[]={-0x20000,-0x8000,0,0x1000,0x8000,0x10000,0x18000,0x30000};
    unsigned n=0;
    for(unsigned w=0;w<sizeof(widths)/sizeof(widths[0]);++w)
        for(unsigned x=0;x<sizeof(offsets)/sizeof(offsets[0]);++x)
            for(unsigned y=0;y<sizeof(offsets)/sizeof(offsets[0]);++y){
                AvpAmp *a=&amps[0];
                memset(amps,0,sizeof(amps));
                a->mode=acting;a->flags=(u16)(1u<<AMP_KILLABLE);a->energy=300;
                a->xpos=(s32)x_pos+offsets[x];a->ypos=(s32)y_pos+offsets[y];
                player_type=PT_PREDATOR;wep_fire=0xffu;seed0=0x1234u;seed1=0x0123u;
                fire_width=widths[w];swing_x=0;centre_angle=0;invisflag=0;
                int hit=(TestSpark()==a);
                mix((uint32_t)hit);mix((uint16_t)a->energy);mix((uint16_t)a->flags);
                mix(fire_angle);mix(seed0);mix(seed1);++n;
            }
    assert(n==640u);
    assert(accum==0xd10bcc390c892fe9ull);
    printf("R249 TestSpark signed-width outcome matrix %u cases hash=%016llx\n",n,(unsigned long long)accum);
    return 0;
}
