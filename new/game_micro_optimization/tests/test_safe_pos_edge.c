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
static uint32_t seed=0xd30a481bu;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned long cases;
static void check(u8 height,s32 x,s32 y,u16 width){
    avp_game_micro_opt_set(0);u8 a=SafePos(height,x,y,width);
    avp_game_micro_opt_set(1);u8 b=SafePos(height,x,y,width);
    if(a!=b){fprintf(stderr,"Mismatch %u %08x %08x %u: %u != %u\n",height,(u32)x,(u32)y,width,a,b);exit(1);}++cases;
}
static s32 xs[8192],ys[8192];
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static volatile unsigned sink;
static void bench(unsigned layout){
    memset(map,0,sizeof(map));memset(fullbits,0,sizeof(fullbits));
    fullbits[1]=AVP_WB_SOLID;fullbits[32]=AVP_WB_DOOR;
    fullbits[33]=AVP_WB_DOOR|AVP_WB_TBDOOR|AVP_WB_EDGEDOOR;
    for(unsigned i=0;i<sizeof(map);i+=8){
        for(unsigned k=0;k<4;k++)map[i+k]=layout==2?32:layout==3?(u8[]){0,1,32,33}[rnd()%4]:0;
        map[i+6]=(u8)(rnd()%129);map[i+7]=(u8)(rnd()%129);
    }
    for(unsigned i=0;i<8192;i++){
        unsigned x=1+rnd()%62,y=1+rnd()%62;
        unsigned fx=0x8000,fy=0x8000;
        if(layout){unsigned slot=rnd()%4;if(slot==0)fx=0x1000;
            if(slot==1)fy=0x1000;
            if(slot==2)fx=0xf000;
            if(slot==3)fy=0xf000;}
        xs[i]=(s32)((x<<16)|fx);ys[i]=(s32)((y<<16)|fy);
    }
    for(unsigned r=0;r<5;r++){
        avp_game_micro_opt_set(1);unsigned sum=0;double t=now();
        for(unsigned i=0;i<6000000;i++)sum+=SafePos(80,xs[i&8191],ys[i&8191],40);
        sink=sum;printf("layout %u %.3f ns checksum %u\n",layout,(now()-t)*1e9/6000000,sink);
    }
}
int main(int argc,char **argv){
    (void)argv;
    if(argc==1){
        avp_collision_bind_maze(map,64,64);
        /* Every flag combination, all open bytes, reflected wall numbers,
         * all four slots, interior & boundary cells; sample orthogonal
         * coordinate and height over their entire domains. */
        for(unsigned flags=0;flags<64;flags++)for(unsigned open=0;open<256;open++)for(unsigned slot=0;slot<4;slot++)for(unsigned mirror=0;mirror<2;mirror++)for(unsigned border=0;border<2;border++){
            unsigned x=border?(slot==2?63:0):32,y=border?(slot==3?63:0):32;
            unsigned cell=(y*64+x)*8;unsigned wall=32+128*mirror;fullbits[wall]=(u8)flags;
            map[cell+slot]=(u8)wall;
            if(slot==0)map[cell+6]=(u8)open;else if(slot==1)map[cell+7]=(u8)open;
            else if(slot==2 && x<63)map[cell+14]=(u8)open;else if(slot==3 && y<63)map[cell+64*8+7]=(u8)open;
            unsigned fx=0x2800u+rnd()%0xb000u,fy=0x2800u+rnd()%0xb000u;
            if(slot==0)fx=0x1000;
            if(slot==1)fy=0x1000;
            if(slot==2)fx=0xf000;
            if(slot==3)fy=0xf000;
            check((u8)rnd(),(s32)((x<<16)|fx),(s32)((y<<16)|fy),20);map[cell+slot]=0;
        }
        const unsigned dims[][2]={{1,1},{1,64},{64,1},{7,19},{64,64},{0,0}};
        for(unsigned k=0;k<6;k++){
            unsigned w=dims[k][0],h=dims[k][1];avp_collision_bind_maze(map,(u16)w,(u16)h);
            for(unsigned i=0;i<256;i++)fullbits[i]=(u8)rnd();
            for(unsigned i=0;i<sizeof(map);i++)map[i]=(u8)rnd();
            for(unsigned i=0;i<250000;i++){
                unsigned x=rnd()%(w+2),y=rnd()%(h+2);u16 width=i%3==0?(u16)rnd():40;
                check((u8)rnd(),(s32)((x<<16)|(rnd()&65535)),(s32)((y<<16)|(rnd()&65535)),width);
                map[rnd()%sizeof(map)]=(u8)rnd();
            }
        }
        /* Local multiplayer switches between different map allocations.
         * Exercise binds both while enabled and disabled, restore the prior
         * map, and mutate live wall bytes without rebinding. */
        static u8 other[64*64*8];
        memset(map,0,sizeof(map));memset(other,0,sizeof(other));
        memset(fullbits,0,sizeof(fullbits));fullbits[1]=AVP_WB_SOLID;
        other[(64+1)*8]=1;
        for(unsigned i=0;i<10000;i++){
            avp_game_micro_opt_set(i&1);avp_collision_bind_maze(i&1?other:map,64,64);
            check(80,0x11000,0x18000,40);
            u8 *m=i&1?other:map;m[(64+1)*8]^=1;
            check(80,0x11000,0x18000,40);
            avp_collision_bind_maze(i&1?map:other,64,64);
            check(80,0x11000,0x18000,40);
        }
        avp_collision_bind_maze(NULL,64,64);check(80,0x18000,0x11000,40);
        printf("SafePos edge parity: %lu cases PASS\n",cases);
    }
    avp_collision_bind_maze(map,64,64);
    seed=0xd30a481bu;for(unsigned i=0;i<4;i++)bench(i);
    return 0;
}
