/* RE31 R11: original AMP collision parity in empty, occupied and edge cells. */
#include "collision.h"
#include "amp.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

u8 fullbits[256];
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
static AvpAmp amps[AVP_NUM_AMPS];
AvpAmp *amp_data=amps;
static uint32_t state=0x8ce49b37u;
static uint32_t rng(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void active(AvpAmp *a){(void)a;}
static unsigned long comparisons;
static int match(s32 x,s32 y){
    int orig,fast;
    avp_game_micro_opt_set(0); orig=AMPCollisions(x,y);
    avp_game_micro_opt_set(1); fast=AMPCollisions(x,y);
    if(orig!=fast){fprintf(stderr,"AMP mismatch (%08x,%08x) old=%d new=%d\n",(unsigned)x,(unsigned)y,orig,fast);return 0;}
    ++comparisons;return 1;
}
int main(void){
    /* Mode and occupancy are separate: stale IDs, invalid owners, and
     * inactive/static actors must still go through the original logic. */
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        amps[i].creature=(u16)(rng()%40u);
        amps[i].mode=(i%3u)?active:NULL;
        amps[i].host_static=(u8)(i%7u==0u);
        amps[i].xpos=(s32)((rng()%64u<<16)|(rng()&0xffffu));
        amps[i].ypos=(s32)((rng()%64u<<16)|(rng()&0xffffu));
    }
    memset(collmap,0,sizeof(collmap));
    for(unsigned i=0;i<1000000u;i++){
        unsigned x=rng()%66u,y=rng()%66u;
        s32 px=(s32)((x<<16)|(rng()&0xffffu));
        s32 py=(s32)((y<<16)|(rng()&0xffffu));
        if(!match(px,py))return 1;
    }
    for(unsigned i=0;i<1000000u;i++){
        unsigned x=rng()%64u,y=rng()%64u,dx=rng()%3u,dy=rng()%3u;
        s32 px=(s32)((x<<16)|(rng()&0xffffu));
        s32 py=(s32)((y<<16)|(rng()&0xffffu));
        unsigned qx=(x+dx>0u)?x+dx-1u:0u;
        unsigned qy=(y+dy>0u)?y+dy-1u:0u;
        if(qx<64u&&qy<64u){
            const unsigned index=qy*64u+qx;
            collmap[index]=(u16)(i%5u==0u?0xffffu:(i%3u==0u?1u+(rng()%AVP_NUM_AMPS):0u));
        }
        if(!match(px,py))return 1;
        if(qx<64u&&qy<64u)collmap[qy*64u+qx]=0;
    }
    for(unsigned i=0;i<500000u;i++){
        const u32 x=rng(),y=rng();
        if(!match((s32)x,(s32)y))return 1;
    }
    /* A real adjacent live AMP and a stale owner ID. */
    memset(collmap,0,sizeof(collmap));
    amp_data[0].xpos=(s32)((20u<<16)|0x8000u);
    amp_data[0].ypos=(s32)((20u<<16)|0x8000u);
    amp_data[0].creature=AC_ALIEN; amp_data[0].mode=active;
    collmap[20u*64u+20u]=1;
    if(!match((s32)((20u<<16)|0x8000u),(s32)((20u<<16)|0x8000u)))return 1;
    if(!match((s32)((21u<<16)|0x8000u),(s32)((20u<<16)|0x8000u)))return 1;
    amp_data[0].mode=NULL;
    if(!match((s32)((20u<<16)|0x8000u),(s32)((20u<<16)|0x8000u)))return 1;
    printf("AMP collision original parity: PASS (%lu comparisons)\n",comparisons);
    return 0;
}
