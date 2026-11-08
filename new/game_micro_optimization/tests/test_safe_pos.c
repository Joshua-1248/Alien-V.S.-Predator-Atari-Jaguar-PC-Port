#include "collision.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Original Collision.S reconstruction depends on the real wall-flag table.
 * This isolated test initializes wall flags in place of a retail ROM. */
u8 fullbits[256];
u8 __real_SafePos(u8 height,s32 xpos,s32 ypos,u16 width);
static uint32_t state=0x93ad758bu;
static uint32_t rng(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"FAIL %s line %d\n",#x,__LINE__);return 1;} }while(0)
int main(void)
{
    u8 map[10*10*8];
    unsigned long n=0;
    const u16 widths[]={0,1,10,20,40,64,80,127,128,255,65535};
    memset(map,0,sizeof(map));
    memset(fullbits,0,sizeof(fullbits));
    fullbits[1]=AVP_WB_SOLID;
    for(unsigned i=0;i<sizeof(map);i+=8) {
        map[i]=(i/8%4==0)?1:0;
        map[i+1]=(i/8%7==0)?1:0;
        map[i+2]=(i/8%11==0)?1:0;
        map[i+3]=(i/8%5==0)?1:0;
    }
    avp_collision_bind_maze(map,10,10);
    for(unsigned wi=0;wi<sizeof(widths)/sizeof(widths[0]);wi++)
      for(unsigned i=0;i<150000;i++){
        s32 x,y;u8 h=(u8)rng();
        if(i%4==0){x=(s32)((rng()%10<<16)|0x8000u);y=(s32)((rng()%10<<16)|0x8000u);}
        else if(i%4==1){x=(s32)((rng()%12<<16)|(rng()&0xffffu));y=(s32)((rng()%12<<16)|(rng()&0xffffu));}
        else {x=(s32)rng();y=(s32)rng();}
        avp_game_micro_opt_set(0); u8 original=SafePos(h,x,y,widths[wi]);
        avp_game_micro_opt_set(1); u8 optimized=SafePos(h,x,y,widths[wi]);
        if(original!=optimized){fprintf(stderr,"diff i=%u width=%u x=%d y=%d orig=%u opt=%u\n",i,widths[wi],x,y,original,optimized);return 1;}
        n++;
      }
    avp_collision_bind_maze(NULL,0,0);
    avp_game_micro_opt_set(1);
    CHECK(SafePos(80,0x8000,0x8000,40)==__real_SafePos(80,0x8000,0x8000,40));
    CHECK(avp_game_micro_opt_get()==1);
    puts("Game Micro-Optimization SafePos parity: PASS");
    printf("Compared %lu cases against original\n",n);
    return 0;
}
