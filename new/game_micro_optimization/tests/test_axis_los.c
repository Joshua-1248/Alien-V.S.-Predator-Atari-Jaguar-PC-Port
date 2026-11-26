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
static void bench(int on){unsigned sum=0;avp_game_micro_opt_set(on);double start=now();for(unsigned i=0;i<2000000;i++)sum+=(unsigned)LineOfSight(&a[i&4095],&b[i&4095]);sink=sum;printf("Axis LOS %s: %.3f ns/call checksum %u\n",on?"ON":"OFF",(now()-start)*500.0,sink);}
/* Directed coverage of each wall slot, reflection, flag combination,
 * door opening and lateral coordinate; both near and multi-cell hits. */
static void directed(void){
    memset(map,0,sizeof(map));avp_collision_bind_maze(map,7,7);
    for(unsigned slot=0;slot<4;slot++)for(unsigned reflected=0;reflected<2;reflected++){
        unsigned x=3,y=3,wi=(y*7+x)*8+slot;
        unsigned di=slot==0?wi+6:slot==1?wi+6:slot==2?wi+12:wi+7*8+4;
        unsigned wall=reflected?129:1;map[wi]=(u8)wall;
        for(unsigned flags=0;flags<64;flags++){
            fullbits[wall]=(u8)flags;
            for(unsigned open=0;open<256;open++){
                map[di]=(u8)open;
                for(unsigned pos=0;pos<128;pos+=7){
                    unsigned f=(pos<<9)|257u;
                    AvpXY p={(s32)((x<<16)|(slot&1?f:0x8000)),(s32)((y<<16)|(slot&1?0x8000:f))},q=p;
                    if(slot==0)q.x-=2<<16;else if(slot==1)q.y-=2<<16;else if(slot==2)q.x+=2<<16;else q.y+=2<<16;
                    check(p,q);
                }
            }
        }
        map[wi]=0;map[di]=0;
    }
}
static void workload_bench(void){
    avp_collision_bind_maze(map,64,64);
    memset(fullbits,0,sizeof(fullbits));fullbits[1]=AVP_WB_SOLID;fullbits[2]=AVP_WB_DOOR;fullbits[3]=AVP_WB_DOOR|AVP_WB_TBDOOR;fullbits[132]=AVP_WB_LEFTONLY;
    for(unsigned layout=0;layout<4;layout++){
        seed=0xa3947ab1u;memset(map,0,sizeof(map));
        for(unsigned i=0;i<64*64*8;i+=8){
            for(unsigned j=0;j<4;j++){
                unsigned r=rnd()%16;
                map[i+j]=layout==0?0:layout==1?(r<3?1:0):layout==2?(r<4?2:r<7?3:0):(r<2?1:r<4?2:r<6?3:r<8?132:0);
            }map[i+6]=(u8)rnd();map[i+7]=(u8)rnd();
        }
        for(unsigned i=0;i<4096;i++){
            u32 x=(rnd()%32<<16)|(0x100u+rnd()%0xfe00),y=(rnd()%32<<16)|(0x100u+rnd()%0xfe00);
            a[i]=(AvpXY){(s32)x,(s32)y};b[i]=a[i];if(i&1)b[i].x+=(s32)((2+rnd()%24)<<16);else b[i].y+=(s32)((2+rnd()%24)<<16);
            if(i&2){AvpXY t=a[i];a[i]=b[i];b[i]=t;}
        }
        for(unsigned rep=0;rep<7;rep++){avp_game_micro_opt_set(1);unsigned sum=0;double start=now();for(unsigned i=0;i<2000000;i++)sum+=(unsigned)LineOfSight(&a[i&4095],&b[i&4095]);sink=sum;printf("layout %u %.3f ns checksum %u\n",layout,(now()-start)*500.0,sink);}
    }
}
int main(int argc,char **argv){(void)argv;if(argc>1){workload_bench();return 0;}
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
                if(v)q.y=(s32)((rnd()%(h+2)<<16)|(i&2?edges[rnd()%8]:rnd()&0xffff));else q.x=(s32)((rnd()%(w+2)<<16)|(i&2?edges[rnd()%8]:rnd()&0xffff));
                check(p,q);check(q,p);
                if(layout)map[rnd()%sizeof(map)]=(u8)rnd();
            }
        }
    }
    avp_collision_bind_maze(NULL,64,64);check((AvpXY){0,0},(AvpXY){0,0});
    directed();
    printf("Axis LOS parity: %u cases PASS\n",cases);
    memset(map,0,sizeof(map));avp_collision_bind_maze(map,64,64);
    for(unsigned i=0;i<4096;i++){
        u32 x=(rnd()%32<<16)|(0x100u+rnd()%0xfe00),y=(rnd()%32<<16)|(0x100u+rnd()%0xfe00);
        a[i]=(AvpXY){(s32)x,(s32)y};b[i]=a[i];if(i&1)b[i].x+=(s32)((2+rnd()%24)<<16);else b[i].y+=(s32)((2+rnd()%24)<<16);
        if(i&2){AvpXY t=a[i];a[i]=b[i];b[i]=t;}
    }
    for(int i=0;i<3;i++){bench(i&1);bench(!(i&1));}
    return 0;
}
