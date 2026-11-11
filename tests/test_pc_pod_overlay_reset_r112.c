/* R112: an escape-pod computer must not inherit the previous terminal's
 * Object Processor art when entering either its inactive or armed menu.
 * Source-only synthetic runtime test: no retail ROM or hardware wave table. */
#include "computer.h"
#include "avp_runtime.h"
#include "joypad.h"
#include "player.h"
#include "doors.h"
#include "levels.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimal original-game runtime dependencies for computer.c. */
volatile u32 joy_cur=0xffffffffu,joy_edge=0xffffffffu;
s16 player_energy=1000,destruct_flag,launch_flag;
u8 acs_level=10,comp_panel;
u16 comp_offset=0x1234u;
s16 cur_level=1;
/* Source has these globals in the renderer/game. Their types follow the
 * public game headers and are intentionally not redeclared differently. */
static AvpRuntimeOps mock_ops;
static unsigned events[32];
static s32 args[32];
static unsigned event_count,vblanks;
const AvpRuntimeOps *avp_runtime_ops(void){return &mock_ops;}
void init_font(void){}
void cursor(void){}
void pr_string_retail(const char *record,u32 addr){(void)record;(void)addr;}
void VSync(void){++vblanks;assert(vblanks<10u);}
void readpad(void)
{
    /* The only input this test offers is a newly pressed C (exit). */
    joy_cur=0xffffffffu&~(1u<<FIRE_C);
    joy_edge=0xffffffffu&~(1u<<FIRE_C);
}
static void record(void *u,unsigned id,s32 a,s32 b)
{
    (void)u;(void)b;
    assert(event_count<sizeof(events)/sizeof(events[0]));
    events[event_count]=id;args[event_count++]=a;
}
static void one_session(int armed)
{
    AvpComputerTerminal terminal={0};
    unsigned clear1=0u,clear2=0u,page=0u;
    event_count=0;vblanks=0;
    destruct_flag=(s16)armed;
    /* Simulate artwork left by a previous schematic computer. */
    NewOver(104u);
    terminal.comp_offset=comp_offset;
    terminal.handler=AVP_COMP_POD;
    terminal.content=NULL;
    avp_computer_bind_level(1u,&terminal,1u);
    cur_level=1;
    Computer();
    for(unsigned i=0;i<event_count;i++){
        if(events[i]==0x434f31u && args[i]==-1)clear1=i+1u;
        if(events[i]==0x434f32u && args[i]==-1)clear2=i+1u;
        if(events[i]==(armed?0x504f444du:0x504f444eu))page=i+1u;
    }
    assert(clear1 && clear2 && page);
    assert(clear1<page && clear2<page);
    assert(!avp_computer_active());
    assert(comp_panel==0);
}
int main(void)
{
    memset(&mock_ops,0,sizeof(mock_ops));
    mock_ops.frontend_event=record;
    one_session(0);
    one_session(1);
    puts("PASS: inactive and armed escape-pod screens discard old OP artwork");
    return 0;
}
