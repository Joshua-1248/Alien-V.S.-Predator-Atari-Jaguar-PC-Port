/* R127: a held LEFT must start retail 32-step wheel motion immediately.
 * Former wait_bit_release() postponed *all* visible motion until keyup. */
#include "mjp.h"
#include "avp_runtime.h"
#include "joypad.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile u32 joy_edge,joy2_edge,joy_cur,joy2_cur;
volatile s8 joy_special_mode,reset_enabled,pad_flag2,pad_flag3;
volatile u16 reset_wait;
volatile u32 reset_time,frame_counter;
void (*x_read)(void);
u32 one_second_ticks=60u;
static unsigned vblanks,first_motion_tick,steps,cross_tick,fade_tick,brief_tick;
void VSync(void){++vblanks;}
void readpad(void)
{
    joy_cur=vblanks<80u?~(1u<<JOY_LEFT):~(1u<<FIRE_A);
}
static void record(void *u,unsigned e,s32 a,s32 b)
{
    (void)u;(void)b;
    if(e==0x53454c56u){
        if(a==PT_PREDATOR||a==PT_HUMAN||a==PT_ALIEN){
            ++steps;
        }
    }
    if(e==0x53454c55u){ (void)a; }
    if(e==0x53474346u)cross_tick=vblanks;
    if(e==0x53474649u)fade_tick=vblanks;
    if(e==0x53454c50u)brief_tick=vblanks;
    /* Every carousel phase after setup is 4*n. Detect the first actual
     * nonzero track phase (not initial zero pose). */
    if(e==0x53454c56u && b!=0 && !first_motion_tick)first_motion_tick=vblanks;
}
int main(void)
{
    AvpRuntimeOps ops;
    AvpMjpObject objects[20];
    memset(&ops,0,sizeof ops);
    memset(objects,0,sizeof objects);
    ops.frontend_event=record;
    avp_runtime_bind(&ops);
    avp_mjp_bind_objects(objects,20u);
    joy_cur=0xffffffffu;
    Do_Selec();
    assert(first_motion_tick>0u && first_motion_tick<30u);
    assert(cross_tick>=80u && cross_tick<130u);
    assert(fade_tick-cross_tick==50u);
    assert(brief_tick-fade_tick==20u);
    assert(steps>=64u);
    printf("PASS R127 SELECT held-left immediate motion at field %u, confirmation at %u; 32-step track preserved\n",
           first_motion_tick,cross_tick);
    return 0;
}
