#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
u8 fullbits[256];
static double sec(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec*1.e-9;}
int main(void){static u8 map[64*64*8];uint32_t sum=0;const unsigned count=20000000;double t0,t1;memset(map,0,sizeof(map));avp_collision_bind_maze(map,64,64);
for(int k=0;k<2;k++){
 avp_game_micro_opt_set(k);t0=sec();
 for(unsigned i=0;i<count;i++){s32 x=(s32)((((i*37u)>>8)%62+1)<<16)|0x8000;
 s32 y=(s32)((((i*51u)>>7)%62+1)<<16)|0x8000;
 sum+=SafePos(80,x,y,40);}
 t1=sec();printf("%s center avg %.3f ns/call checksum %u\n",k?"ON ":"OFF",(t1-t0)*1.e9/count,sum);
}
for(int k=0;k<2;k++){
 avp_game_micro_opt_set(k);t0=sec();
 for(unsigned i=0;i<count;i++){s32 x=(s32)((((i*37u)>>8)%62+1)<<16)|0x0800;
 s32 y=(s32)((((i*51u)>>7)%62+1)<<16)|0x7000;
 sum+=SafePos(80,x,y,40);}
 t1=sec();printf("%s edge avg %.3f ns/call checksum %u\n",k?"ON ":"OFF",(t1-t0)*1.e9/count,sum);
}
return (int)(sum==0xffffffffu);}
