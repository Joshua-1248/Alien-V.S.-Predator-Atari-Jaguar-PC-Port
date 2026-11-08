#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "amp.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
u8 fullbits[256];
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
static AvpAmp amps[AVP_NUM_AMPS];
AvpAmp *amp_data=amps;
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec*1e-9;}
static void run(const char *name,int occupied){
    const unsigned n=12000000u;
    static const unsigned count=128u;
    static s32 xs[128],ys[128];
    for(unsigned i=0;i<count;i++){
        const unsigned x=2u+(i*23u)%60u,y=2u+(i*37u)%60u;
        xs[i]=(s32)((x<<16)|0x8000u);ys[i]=(s32)((y<<16)|0x8000u);
    }
    memset(collmap,0,sizeof(collmap));
    if(occupied)for(unsigned i=0;i<count;i++){
        const unsigned x=(u32)xs[i]>>16,y=(u32)ys[i]>>16;
        collmap[y*AVP_AMP_GRID_W+x]=(u16)(AVP_NUM_AMPS+1u);
    }
    for(int enabled=0;enabled<=1;enabled++){
        unsigned sum=0;double start;
        avp_game_micro_opt_set(enabled);
        start=seconds();
        for(unsigned i=0;i<n;i++) sum+=(unsigned)AMPCollisions(xs[i&(count-1u)],ys[i&(count-1u)]);
        printf("%s %s: %.2f ns/call checksum=%u\n",name,enabled?"optimized":"original",(seconds()-start)*1e9/n,sum);
    }
}
int main(void){run("empty-neighborhood",0);run("nonzero-owner fallback",1);return 0;}
