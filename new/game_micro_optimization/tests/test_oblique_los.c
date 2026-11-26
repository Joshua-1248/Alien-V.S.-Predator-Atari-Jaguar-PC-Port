#define _POSIX_C_SOURCE 200809L
#include "collision.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
u8 fullbits[256];
static u8 map[80*80*8];
static uint32_t seed=0xa3947ab1u;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned cases;
static void check(AvpXY a,AvpXY b){
    avp_game_micro_opt_set(0);int x=LineOfSight(&a,&b);
    avp_game_micro_opt_set(1);int y=LineOfSight(&a,&b);
    if(x!=y){fprintf(stderr,"Mismatch %08x,%08x -> %08x,%08x: %d vs %d\n",(u32)a.x,(u32)a.y,(u32)b.x,(u32)b.y,x,y);exit(1);}++cases;
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static AvpXY a[4096],b[4096];
static volatile unsigned sink;
static void bench(int on){unsigned sum=0;avp_game_micro_opt_set(on);double start=now();for(unsigned i=0;i<2000000;i++)sum+=(unsigned)LineOfSight(&a[i&4095],&b[i&4095]);sink=sum;printf("Oblique LOS %s: %.3f ns/call checksum %u\n",on?"ON":"OFF",(now()-start)*500.0,sink);}
int main(void){
    fullbits[1]=AVP_WB_SOLID;fullbits[2]=AVP_WB_DOOR;fullbits[3]=AVP_WB_LEFTONLY;fullbits[4]=AVP_WB_2SIDES;fullbits[129]=AVP_WB_SOLID;
    const unsigned dims[][2]={{1,1},{1,64},{64,1},{7,19},{64,64},{80,80}};
    const unsigned edges[]={0,1,2,0x7fff,0x8000,0xfffd,0xfffe,0xffff};
    for(unsigned k=0;k<6;k++){
        unsigned w=dims[k][0],h=dims[k][1];avp_collision_bind_maze(map,w,h);
        for(unsigned layout=0;layout<3;layout++){
            for(unsigned i=0;i<sizeof(map);i++)map[i]=layout==0?0:layout==1?(rnd()%9?0:(u8)(1+rnd()%4)):(u8)rnd();
            for(unsigned i=0;i<100000;i++){
                unsigned v=i&1;u32 x=(rnd()%(w+2)<<16)|edges[rnd()%8],y=(rnd()%(h+2)<<16)|edges[rnd()%8];
                AvpXY p={(s32)x,(s32)y},q=p;
                (void)v; q.y=(s32)((rnd()%(h+2)<<16)|(i&2?edges[rnd()%8]:rnd()&0xffff)); q.x=(s32)((rnd()%(w+2)<<16)|(i&2?edges[rnd()%8]:rnd()&0xffff));
                check(p,q);check(q,p);
                if(layout)map[rnd()%sizeof(map)]=(u8)rnd();
            }
        }
    }
    avp_collision_bind_maze(NULL,64,64);check((AvpXY){0,0},(AvpXY){0,0});
    printf("Oblique LOS parity: %u cases PASS\n",cases);
    memset(map,0,sizeof(map));avp_collision_bind_maze(map,64,64);
    for(unsigned i=0;i<4096;i++){
        u32 x=(rnd()%32<<16)|(0x100u+rnd()%0xfe00),y=(rnd()%32<<16)|(0x100u+rnd()%0xfe00);
        a[i]=(AvpXY){(s32)x,(s32)y};b[i]=a[i];b[i].x+=(s32)((2+rnd()%24)<<16);b[i].y+=(s32)((2+rnd()%24)<<16);
        if(i&2){AvpXY t=a[i];a[i]=b[i];b[i]=t;}
    }
    for(int i=0;i<3;i++){bench(i&1);bench(!(i&1));}
    for(unsigned layout=1;layout<=2;layout++){
        for(unsigned j=0;j<sizeof(map);j++)map[j]=(rnd()%(layout==1?32u:5u))?0:1;
        printf("Mixed wall layout %u\n",layout);
        for(int i=0;i<3;i++){bench(i&1);bench(!(i&1));}
    }
    return 0;
}
