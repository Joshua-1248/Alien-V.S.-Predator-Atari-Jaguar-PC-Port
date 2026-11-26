#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "game_micro_optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
static uint32_t seed=0xc329ad12u;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void check_vec(AvpXY a,AvpXY b){
    s32 x,y,u,v;
    avp_game_micro_opt_set(0);Vector(&a,&b,&x,&y);
    avp_game_micro_opt_set(1);Vector(&a,&b,&u,&v);
    CHECK(x==u && y==v);
    /* The API allows the two output addresses to alias. */
    avp_game_micro_opt_set(0);Vector(&a,&b,&x,&x);
    avp_game_micro_opt_set(1);Vector(&a,&b,&u,&u);CHECK(x==u);
}
static void check_move(u16 x,u16 y,u8 d,u16 wanted){
    avp_game_micro_opt_set(0);u16 a=NMoves(x,y,d,wanted);
    avp_game_micro_opt_set(1);u16 b=NMoves(x,y,d,wanted);
    CHECK(a==b);
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static volatile uint64_t sink;
static AvpXY aa[4096],bb[4096];
static void bench_vec(int enabled){
    avp_game_micro_opt_set(enabled);uint64_t sum=0;double t=now();
    for(unsigned i=0;i<6000000;i++){s32 x,y;Vector(&aa[i&4095],&bb[i&4095],&x,&y);sum+=(u32)x+(u32)y;}
    double dt=now()-t;sink=sum;printf("Vector %s: %.3f ns/call checksum %llu\n",enabled?"ON":"OFF",dt*1e9/6000000,(unsigned long long)sink);
}
static void bench_move(int enabled){
    avp_game_micro_opt_set(enabled);uint64_t sum=0;double t=now();
    for(unsigned i=0;i<6000000;i++)sum+=NMoves(aa[i&4095].x&63,aa[i&4095].y&63,(u8)i,16);
    double dt=now()-t;sink=sum;printf("NMoves %s: %.3f ns/call checksum %llu\n",enabled?"ON":"OFF",dt*1e9/6000000,(unsigned long long)sink);
}
int main(void){
    static u8 map[80*80*8];
    const u32 edge[]={0,1,2,3,0x7fff,0x8000,0xffff,0x10000,0x3fffffff,0x40000000,0x7fffffff,0x80000000,0x80000001,0xffffffff};
    for(unsigned i=0;i<sizeof(edge)/sizeof(*edge);i++)for(unsigned j=0;j<sizeof(edge)/sizeof(*edge);j++)check_vec((AvpXY){0,0},(AvpXY){(s32)edge[i],(s32)edge[j]});
    for(unsigned i=0;i<1500000;i++){
        AvpXY a={(s32)rnd(),(s32)rnd()},b={(s32)rnd(),(s32)rnd()};
        if(i&1){a.x&=0x3fffff;a.y&=0x3fffff;b.x&=0x3fffff;b.y&=0x3fffff;}
        check_vec(a,b);
    }
    printf("Vector: 1500196 coordinate pairs, including aliased outputs, PASS\n");
    const unsigned dims[][2]={{1,1},{1,64},{64,1},{7,19},{64,64},{80,80},{0,0}};
    for(unsigned k=0;k<sizeof(dims)/sizeof(*dims);k++){
        unsigned w=dims[k][0],h=dims[k][1];
        for(unsigned layout=0;layout<4;layout++){
            for(unsigned i=0;i<sizeof(map);i++)map[i]=layout==0?0:layout==1?(u8)rnd():layout==2?31:32;
            avp_collision_bind_maze(map,w,h);
            for(unsigned i=0;i<50000;i++){
                u16 x=(u16)(rnd()%(w+2)),y=(u16)(rnd()%(h+2));
                u8 d=(u8)rnd();u16 want=(i&1)?(u16)rnd():(u16)(rnd()%20);
                check_move(x,y,d,want);
                /* Update live wall values after each comparison. */
                map[rnd()%sizeof(map)]=(u8)rnd();
            }
        }
    }
    avp_collision_bind_maze(NULL,64,64);check_move(1,1,3,9);
    puts("NMoves: 1400001 cases, including live edits, all directions, borders and fallback, PASS");
    for(unsigned i=0;i<4096;i++){aa[i]=(AvpXY){(s32)(rnd()&0x3fffff),(s32)(rnd()&0x3fffff)};bb[i]=(AvpXY){(s32)(rnd()&0x3fffff),(s32)(rnd()&0x3fffff)};}
    memset(map,0,sizeof(map));avp_collision_bind_maze(map,64,64);
    for(unsigned r=0;r<3;r++){bench_vec(r&1);bench_vec(!(r&1));bench_move(r&1);bench_move(!(r&1));}
    return 0;
}
