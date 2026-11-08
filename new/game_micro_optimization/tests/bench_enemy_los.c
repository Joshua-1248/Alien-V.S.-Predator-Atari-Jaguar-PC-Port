#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
u8 fullbits[256];
static double now_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec*1e-9;}
static volatile uint32_t checksum;
static void run_case(const char *label, int near, int optimized)
{
    const unsigned count=2000000u;
    uint32_t sum=0;
    AvpXY a,b;
    double t=now_s();
    avp_game_micro_opt_set(optimized);
    for(unsigned i=0;i<count;i++) {
        /* Different endpoints per iteration prevent constant folding. */
        unsigned x=(i*17u/101u)%62u+1u,y=(i*23u/101u)%62u+1u;
        a.x=(s32)((x<<16)|((i*31u)%0x1000u+0x7100u));
        a.y=(s32)((y<<16)|((i*37u)%0x1000u+0x7100u));
        if(near){b.x=(s32)((x<<16)|((i*47u)%0x1000u+0x7100u));b.y=(s32)((y<<16)|((i*53u)%0x1000u+0x7100u));}
        else {b.x=(s32)(((x+8u)%62u+1u)<<16|((i*47u)%0x1000u+0x7100u));b.y=(s32)(((y+4u)%62u+1u)<<16|((i*53u)%0x1000u+0x7100u));}
        sum+=(uint32_t)LineOfSight(&a,&b);
    }
    t=now_s()-t;
    checksum+=sum;
    printf("%s %s: %.2f ns/call  checksum=%u\n",label,optimized?"on ":"off",t*1e9/(double)count,sum);
}
int main(void)
{
    static u8 maze[64*64*8];
    memset(maze,0,sizeof(maze));memset(fullbits,0,sizeof(fullbits));
    avp_collision_bind_maze(maze,64,64);
    run_case("same-cell",1,0);run_case("same-cell",1,1);
    run_case("far-cell ",0,0);run_case("far-cell ",0,1);
    return (checksum==0xffffffffu);
}
