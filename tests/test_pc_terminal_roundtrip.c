/* Host-side interaction test of the unmodified retail Computer() state machine. */
#include "computer.h"
#include "avp_runtime.h"
#include "joypad.h"
#include "maze.h"
#include "player.h"
#include "levels.h"
#include "doors.h"
#include "pc/pc_platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimal host-owned game globals needed for isolated original computer loop. */
s16 cur_level,player_energy,destruct_flag,launch_flag;
u8 acs_level,comp_panel;
u16 comp_offset;
static const char page0[]={0,10,0,30,'F','I','R','S','T',0};
static const char page1[]={0,10,0,30,'S','E','C','O','N','D',0};
static const u32 addr[]={0x123400u,0x123500u};
static const AvpComputerContent content={2,addr,NULL,0};
static const AvpComputerTerminal entry={0x7788u,AVP_COMP_GENERIC,&content};
static int stage=-1,stage_frames=0,exited,seen[2],menu_count,release_loops,held_button;
static unsigned long polls,frames;
static u32 currently_pressed;
static void press(unsigned bit){currently_pressed=1u<<bit;avp_pc_pad_set(0xffffffffu&~currently_pressed);}
static void release(void){currently_pressed=0;avp_pc_pad_set(0xffffffffu);}
static void poll_pad(void)
{
    ++polls;
    if(held_button){
        /* Native Computer() must see release before another VSync: this was
         * the actual Linux freeze before PC-host per-read polling. */
        if(++release_loops>=3){held_button=0;release();}
    }
}
static void vblank(void *unused)
{
    (void)unused;
    ++frames;
    assert(frames<250);
    ++stage_frames;
    release();
    if(stage==0&&stage_frames==1)press(JOY_DOWN);
    if(stage==1&&stage_frames==1)press(FIRE_B);
    if(stage==2&&stage_frames==2){press(FIRE_A);held_button=1;release_loops=0;}
    if(stage==3&&stage_frames==2){press(FIRE_B);held_button=1;release_loops=0;}
    if(stage==4&&stage_frames==2)press(FIRE_C);
    if(stage==5&&stage_frames==2)press(FIRE_C);
}
static void event(void *u,unsigned tag,s32 a,s32 b)
{
    (void)u;(void)b;
    if(tag==0x47454e4du){ /* GENM */
        ++menu_count;stage=menu_count==1?0:5;stage_frames=0;
    }
    if(tag==0x43555253u && a==1&&stage==0){stage=1;stage_frames=0;}
    if(tag==0x4350414cu&&!a)exited=1;
}
static const char *text_resource(void*u,u32 addr_in)
{
    (void)u;
    if(addr_in==addr[0]){++seen[0];if(stage==3){stage=4;stage_frames=0;}else {stage=2;stage_frames=0;}return page0;}
    if(addr_in==addr[1]){++seen[1];stage=3;stage_frames=0;return page1;}
    return NULL;
}
/* Host-independent test of original terminal loop, with only PC pad polling
 * and runtime callback seams mocked. No Jaguar-derived functions modified. */
int main(void)
{
    AvpRuntimeOps o;
    memset(&o,0,sizeof(o));
    o.wait_vblank=vblank;
    o.frontend_event=event;
    o.frontend_text=text_resource;
    avp_runtime_bind(&o);
    initpad();avp_pc_pad_set_poll_callback(poll_pad);release();
    avp_computer_bind_level(1,&entry,1u);
    cur_level=1;comp_offset=entry.comp_offset;
    Computer();
    assert(exited);
    assert(seen[0]==2&&seen[1]==1);
    assert(menu_count==2);
    assert(release_loops>=3);
    assert(!avp_computer_active());
    printf("Original terminal navigation: page 0 -> next -> previous -> exit PASS (%lu pad polls, %lu frames).\n",polls,frames);
    return 0;
}
