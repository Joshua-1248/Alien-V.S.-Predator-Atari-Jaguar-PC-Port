#define _POSIX_C_SOURCE 200809L
#include "local_mp_amp_native.h"
#include "amp_free_list.h"
#include "avp_runtime.h"
#include "collision.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

s16 cur_level;
u32 x_pos,y_pos;
s16 player_energy;
const AvpRuntimeOps *avp_runtime_ops(void){return NULL;}
u8 SafePos(u8 height,s32 x,s32 y,u16 width)
{(void)height;(void)x;(void)y;(void)width;return 0;}
void AreaAmpDamage(s32 x,s32 y,s32 r,s16 d,u16 f)
{(void)x;(void)y;(void)r;(void)d;(void)f;}
void AreaDamage(s32 x,s32 y,s32 r,s16 d,u16 f)
{(void)x;(void)y;(void)r;(void)d;(void)f;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+1e-9*t.tv_nsec;}
static volatile unsigned long long checksum;
static void sample(const char *label,int fast,unsigned count)
{
    static AvpMpAmpNativeState snap;
    double start;
    unsigned long long c=0;
    avp_mp_amp_native_set_direct_capture(fast);
    start=now();
    for(unsigned i=0;i<count;i++){
        if(!avp_mp_amp_native_capture(&snap)){fprintf(stderr,"capture failed\n");return;}
        c+=(unsigned)snap.valid+(unsigned)snap.free_count+(unsigned)snap.actors[0].creature;
    }
    double elapsed=now()-start;
    checksum+=c;
    printf("%s: %.3f us/capture, count=%u checksum=%llu\n",label,elapsed*1e6/count,count,c);
}
int main(void)
{
    initialise();
    static AvpMpAmpNativeState w;
    if(!avp_mp_amp_native_capture(&w))return 1;
    w.free_count=AVP_NUM_AMPS;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++)w.free_list[i]=(u16)(AVP_NUM_AMPS-1-i);
    if(!avp_mp_amp_native_restore(&w))return 2;
    avp_mp_amp_native_set_free_list_validator(avp_game_micro_amp_free_list_valid);
    printf("AMP snapshot bytes: %zu\n",sizeof(w));
    for(unsigned round=0;round<4;round++){
        if(round&1u){sample("R14 direct",1,5000);sample("R13 staged",0,5000);}
        else {sample("R13 staged",0,5000);sample("R14 direct",1,5000);}
    }
    return 0;
}
