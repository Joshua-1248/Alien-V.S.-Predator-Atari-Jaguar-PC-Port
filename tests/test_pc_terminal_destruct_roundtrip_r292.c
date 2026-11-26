/* Original COMPUTER.S Command Center terminal: cursor, warning, arm, exit.
 * Unlike direct flag-unit tests, this drives the retail terminal's entire
 * synchronous input state machine and checks no phantom pod launch occurs. */
#include "computer.h"
#include "avp_runtime.h"
#include "joypad.h"
#include "maze.h"
#include "player.h"
#include "levels.h"
#include "doors.h"
#include "pc/pc_platform.h"
#include <stdio.h>
#include <string.h>

s16 cur_level,player_energy,destruct_flag,launch_flag;
u8 acs_level,comp_panel;
u16 comp_offset;
static int stage,frame_stage,menus,warnings,armed,returned,sfx_count;
static unsigned frames;
static void pad(unsigned bit){avp_pc_pad_set(~(1u<<bit));}
static void release(void){avp_pc_pad_set(UINT32_MAX);}
static void field(void *unused)
{
    (void)unused;
    if(++frames>100u){fprintf(stderr,"terminal input loop timed out: stage %d\n",stage);__builtin_trap();}
    release();
    switch(stage){
    case 1: /* First GENM: down twice with release between each, then B. */
        if(frame_stage==0||frame_stage==2)pad(JOY_DOWN);
        else if(frame_stage==4)pad(FIRE_B);
        break;
    case 2: /* Warning: A confirms the actual original destruct command. */
        if(frame_stage==0)pad(FIRE_A);
        break;
    case 3: /* Armed notice: C returns. */
        if(frame_stage==0)pad(FIRE_C);
        break;
    case 4: /* Second GENM: release C before a new press to exit. */
        if(frame_stage==1)pad(FIRE_C);
        break;
    }
    ++frame_stage;
}
static void event(void *unused,unsigned tag,s32 a,s32 b)
{
    (void)unused;(void)b;
    if(tag==0x47454e4du){
        ++menus;
        stage=menus==1?1:4;frame_stage=0;
        if(menus==1 && a!=3)__builtin_trap();
        if(menus==2 && a!=2)__builtin_trap();
    }else if(tag==0x5741524eu){++warnings;stage=2;frame_stage=0;}
    else if(tag==0x44455354u){++armed;stage=3;frame_stage=0;}
    else if(tag==0x4350414cu && !a){++returned;stage=5;}
}
static void sfx(void *unused,unsigned id){(void)unused;(void)id;++sfx_count;}
int main(void)
{
    AvpRuntimeOps o={0};
    o.wait_vblank=field;o.frontend_event=event;o.play_sfx=sfx;
    avp_runtime_bind(&o);
    avp_computer_bind_retail_terminals();
    initpad();release();
    cur_level=2;comp_offset=6299u;comp_panel=2;acs_level=10;
    Computer();
    if(menus!=2||warnings!=1||armed!=1||returned!=1||
       !destruct_flag||launch_flag||sfx_count!=1||
       avp_computer_active()||comp_panel){
       fprintf(stderr,"destruct roundtrip failed: menus=%d warning=%d armed=%d return=%d destruct=%d launch=%d sfx=%d\n",
            menus,warnings,armed,returned,destruct_flag,launch_flag,sfx_count);
       return 1;
    }
    printf("PASS: retail Command Center terminal warning -> arm -> close (%u fields)\n",frames);
    return 0;
}
