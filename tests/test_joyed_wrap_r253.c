#include "joyed.h"
#include "joypad.h"
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
volatile u32 joy_edge=0xffffffffu,joy2_edge,joy_cur=0xffffffffu,joy2_cur;
static s32 wrap(u32 bits){return bits<=INT32_MAX?(s32)bits:(s32)((int64_t)bits-INT64_C(4294967296));}
static int check(int special, s32 start, unsigned mag, unsigned control, unsigned mask){
    JoyEditRecord r={0};r.value=start;r.magnitude=(u16)mag;
    r.pad_0a[special?9:3]=(u8)control;
    joy_edge=~((u32)mask<<20);
    unsigned shift=(unsigned)(u8)(6u-(u8)control)&31u;
    u32 step=(((u32)(u16)mag)>>shift)<<3;
    u32 bits=(u32)start;
    if(mask&1u)bits-=step;
    if(mask&2u)bits+=step;
    if(mask&4u)--bits;
    if(mask&8u)++bits;
    MoveData(&r,special?(const void *)(uintptr_t)0x203f8u:&r);
    if(r.value!=wrap(bits)){
        fprintf(stderr,"JOYED mismatch special=%d start=%d mag=%u control=%u mask=%u observed=%d expected=%d\n",special,start,mag,control,mask,r.value,wrap(bits));return 1;
    }
    return 0;
}
int main(void){
    const s32 values[]={0,1,-1,17,-33,INT32_MAX,INT32_MAX-1,INT32_MIN,INT32_MIN+1,0x40000000,-0x40000000};
    const unsigned mags[]={0,1,2,15,32767,65535};
    const unsigned ctl[]={0,1,5,6,7,11,128,255};
    unsigned n=0;
    for(int sp=0;sp<2;sp++)for(unsigned vi=0;vi<sizeof values/sizeof *values;vi++)for(unsigned mi=0;mi<sizeof mags/sizeof *mags;mi++)for(unsigned ci=0;ci<sizeof ctl/sizeof *ctl;ci++)for(unsigned mask=0;mask<16;mask++){
        if(check(sp,values[vi],mags[mi],ctl[ci],mask))return 1;
        n++;
    }
    printf("JOYED wrapped value parity PASS (%u cases)\n",n);
    return 0;
}
